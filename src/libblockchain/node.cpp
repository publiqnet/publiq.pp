#include "node.hpp"
#include "common.hpp"

#include "node_internals.hpp"

#include "communication_rpc.hpp"
#include "communication_p2p.hpp"

#include "open_container_packet.hpp"

#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <exception>

using namespace BlockchainMessage;

namespace filesystem = boost::filesystem;

using beltpp::ip_address;
using beltpp::ip_destination;
using beltpp::socket;
using beltpp::packet;
using peer_id = socket::peer_id;

namespace chrono = std::chrono;
using chrono::system_clock;
using chrono::steady_clock;

using std::pair;
using std::string;
using std::vector;
using std::unique_ptr;
using std::unordered_set;
using std::unordered_map;

namespace publiqpp
{

/*
 * node
 */
node::node(ip_address const& rpc_bind_to_address,
           ip_address const& p2p_bind_to_address,
           std::vector<ip_address> const& p2p_connect_to_addresses,
           boost::filesystem::path const& fs_blockchain,
           boost::filesystem::path const& fs_action_log,
           boost::filesystem::path const& fs_storage,
           boost::filesystem::path const& fs_transaction_pool,
           boost::filesystem::path const& fs_state,
           beltpp::ilog* plogger_p2p,
           beltpp::ilog* plogger_node,
           meshpp::private_key const& pv_key,
           bool log_enabled)
    : m_pimpl(new detail::node_internals(rpc_bind_to_address,
                                         p2p_bind_to_address,
                                         p2p_connect_to_addresses,
                                         fs_blockchain,
                                         fs_action_log,
                                         fs_storage,
                                         fs_transaction_pool,
                                         fs_state,
                                         plogger_p2p,
                                         plogger_node,
                                         pv_key,
                                         log_enabled))
{

}

node::node(node&&) = default;

node::~node()
{

}

void node::terminate()
{
    m_pimpl->m_ptr_eh->terminate();
}

string node::name() const
{
    return m_pimpl->m_ptr_p2p_socket->name();
}

bool node::run()
{
    bool code = true;

    // temp solution for genesis
    // will return id chain is not empty
    insert_genesis(m_pimpl);

    unordered_set<beltpp::ievent_item const*> wait_sockets;

    auto wait_result = m_pimpl->m_ptr_eh->wait(wait_sockets);

    enum class interface_type {p2p, rpc};

    if (wait_result == beltpp::event_handler::event)
    {
        for (auto& pevent_item : wait_sockets)
        {
            interface_type it = interface_type::rpc;
            if (pevent_item == &m_pimpl->m_ptr_p2p_socket.get()->worker())
                it = interface_type::p2p;

            auto str_receive = [it]
            {
                if (it == interface_type::p2p)
                    return "p2p_sk.receive";
                else
                    return "rpc_sk.receive";
            };
            str_receive();

            auto str_peerid = [it](string const& peerid)
            {
                B_UNUSED(it);
                /*if (it == interface_type::p2p)
                    return peerid.substr(0, 5);
                else*/
                return peerid;
            };

            beltpp::socket::peer_id peerid;

            beltpp::isocket* psk = nullptr;
            if (pevent_item == &m_pimpl->m_ptr_p2p_socket->worker())
                psk = m_pimpl->m_ptr_p2p_socket.get();
            else if (pevent_item == m_pimpl->m_ptr_rpc_socket.get())
                psk = m_pimpl->m_ptr_rpc_socket.get();

            beltpp::socket::packets received_packets;
            if (psk != nullptr)
                received_packets = psk->receive(peerid);

            for (auto& received_packet : received_packets)
            {
            try
            {
                vector<packet*> composition;

                open_container_packet<Broadcast, SignedTransaction> broadcast_transaction;
                open_container_packet<Broadcast> broadcast_anything;
                bool is_container = broadcast_transaction.open(received_packet, composition) ||
                                    broadcast_anything.open(received_packet, composition);

                if (is_container == false)
                {
                    composition.clear();
                    composition.push_back(&received_packet);
                }

                packet& ref_packet = *composition.back();

                packet stored_packet;
                if (it == interface_type::p2p)
                    m_pimpl->find_stored_request(peerid, stored_packet);
                
                switch (ref_packet.type())
                {
                case beltpp::isocket_join::rtt:
                {
                    m_pimpl->write_node(str_peerid(peerid));
                    m_pimpl->writeln_node("joined");

                    if (psk == m_pimpl->m_ptr_p2p_socket.get())
                    {
                        beltpp::on_failure guard(
                            [&peerid, &psk] { psk->send(peerid, beltpp::isocket_drop()); });

                        m_pimpl->add_peer(peerid);

                        guard.dismiss();
                    }
                    break;
                }
                case beltpp::isocket_drop::rtt:
                {
                    m_pimpl->write_node(str_peerid(peerid));
                    m_pimpl->writeln_node("dropped");

                    if (psk == m_pimpl->m_ptr_p2p_socket.get())
                        m_pimpl->remove_peer(peerid);

                    break;
                }
                case beltpp::isocket_protocol_error::rtt:
                {
                    beltpp::isocket_protocol_error msg;
                    ref_packet.get(msg);
                    m_pimpl->write_node(str_peerid(peerid));
                    m_pimpl->writeln_node("protocol error");
                    m_pimpl->writeln_node(msg.buffer);
                    psk->send(peerid, beltpp::isocket_drop());

                    if (psk == m_pimpl->m_ptr_p2p_socket.get())
                        m_pimpl->remove_peer(peerid);

                    break;
                }
                case beltpp::isocket_open_refused::rtt:
                {
                    beltpp::isocket_open_refused msg;
                    ref_packet.get(msg);
                    m_pimpl->write_node(peerid);
                    m_pimpl->writeln_node(msg.reason);
                    break;
                }
                case beltpp::isocket_open_error::rtt:
                {
                    beltpp::isocket_open_error msg;
                    ref_packet.get(msg);
                    m_pimpl->write_node(peerid);
                    m_pimpl->writeln_node(msg.reason);
                    break;
                }
                case Shutdown::rtt:
                {
                    if (it != interface_type::rpc)
                        break;

                    m_pimpl->writeln_node("shutdown received");

                    code = false;

                    psk->send(peerid, Done());

                    if (false == broadcast_anything.items.empty())
                    {
                        m_pimpl->writeln_node("broadcasting shutdown");

                        Shutdown shutdown_msg;
                        ref_packet.get(shutdown_msg);

                        for (auto const& p2p_peer : m_pimpl->m_p2p_peers)
                            m_pimpl->m_ptr_p2p_socket->send(p2p_peer, shutdown_msg);
                    }
                    break;
                }
                case Transfer::rtt:
                {
                    if (broadcast_transaction.items.empty())
                        throw std::runtime_error("will process only \"broadcast signed transaction\"");
                
                    process_transfer(*broadcast_transaction.items[1],
                                     ref_packet,
                                     m_pimpl);


                    broadcast(received_packet,
                              m_pimpl->m_ptr_p2p_socket->name(),
                              peerid,
                              (it == interface_type::rpc),
                              //m_pimpl->plogger_node,
                              nullptr,  
                              m_pimpl->m_p2p_peers,
                              m_pimpl->m_ptr_p2p_socket.get());
                
                    if (it == interface_type::rpc)
                        psk->send(peerid, Done());

                    break;
                }
                case RevertLastLoggedAction::rtt:
                {
                    if (it == interface_type::rpc)
                    {
                        beltpp::on_failure guard([&] { m_pimpl->discard(); });

                        m_pimpl->m_action_log.revert();

                        m_pimpl->save(guard);

                        psk->send(peerid, Done());
                    }
                    break;
                }
                case StorageFile::rtt:
                {
                    StorageFile file;
                    std::move(ref_packet).get(file);
                    StorageFileAddress addr;
                    addr.uri = m_pimpl->m_storage.put(std::move(file));
                    psk->send(peerid, std::move(addr));
                    break;
                }
                case StorageFileAddress::rtt:
                {
                    StorageFileAddress addr;
                    std::move(ref_packet).get(addr);
                    StorageFile file;
                    if (m_pimpl->m_storage.get(addr.uri, file))
                        psk->send(peerid, std::move(file));
                    else
                    {
                        FileNotFound error;
                        error.uri = addr.uri;
                        psk->send(peerid, std::move(error));
                    }
                    break;
                }
                case LoggedTransactionsRequest::rtt:
                {
                    if (it == interface_type::rpc)
                        get_actions(ref_packet, m_pimpl->m_action_log, *psk, peerid);
                    break;
                }
                case DigestRequest::rtt:
                {
                    get_hash(ref_packet, *psk, peerid);
                    break;
                }
                case MasterKeyRequest::rtt:
                {
                    get_random_seed(*psk, peerid);
                    break;
                }
                case KeyPairRequest::rtt:
                {
                    get_key_pair(ref_packet, *psk, peerid);
                    break;
                }
                case SignRequest::rtt:
                {
                    get_signature(ref_packet, *psk, peerid);
                    break;
                }
                case Signature::rtt:
                {
                    verify_signature(ref_packet, *psk, peerid);
                    break;
                }
                case SyncRequest::rtt:
                {
                    if (it != interface_type::p2p)
                        wrong_request_exception("SyncRequest  received through rpc!");

                    BlockHeader block_header;
                    m_pimpl->m_blockchain.header(block_header);

                    SyncResponse sync_response;
                    sync_response.block_number = block_header.block_number;
                    sync_response.consensus_sum = block_header.c_sum;

                    psk->send(peerid, std::move(sync_response));

                    break;
                }
                case SyncResponse::rtt:
                {
                    if (it != interface_type::p2p)
                        wrong_request_exception("SyncResponse received through rpc!");

                    m_pimpl->reset_stored_request(peerid);
                    if (stored_packet.type() != SyncRequest::rtt)
                        throw wrong_data_exception("SyncResponse");

                    SyncResponse sync_response;
                    std::move(ref_packet).get(sync_response);
                    m_pimpl->sync_responses.push_back(std::pair<beltpp::isocket::peer_id, SyncResponse>(peerid, sync_response));

                    break;
                }
                case BlockHeaderRequest::rtt:
                {
                    if (it != interface_type::p2p)
                        wrong_request_exception("BlockHeaderRequest received through rpc!");

                    process_blockheader_request(ref_packet, m_pimpl, *psk, peerid);

                    break;
                }
                case BlockHeaderResponse::rtt:
                {
                    if (it != interface_type::p2p)
                        wrong_request_exception("BlockHeaderResponse received through rpc!");

                    m_pimpl->reset_stored_request(peerid);
                    if (stored_packet.type() != BlockHeaderRequest::rtt)
                        throw wrong_data_exception("BlockHeaderResponse");

                    if(m_pimpl->sync_peerid == peerid)
                        process_blockheader_response(ref_packet, m_pimpl, *psk, peerid);

                    break;
                }
                case BlockChainRequest::rtt:
                {
                    if (it == interface_type::p2p)
                        wrong_request_exception("BlockChainRequest received through rpc!");

                    process_blockchain_request(ref_packet, m_pimpl, *psk, peerid);

                    break;
                }
                case BlockChainResponse::rtt:
                {
                    if (it == interface_type::p2p)
                        wrong_request_exception("BlockChainResponse received through rpc!");

                    m_pimpl->reset_stored_request(peerid);
                    if (stored_packet.type() != BlockChainRequest::rtt)
                        throw wrong_data_exception("BlockChainResponse");

                    if (m_pimpl->sync_peerid == peerid)
                        process_blockchain_response(ref_packet, m_pimpl, *psk, peerid);

                    break;
                }
                default:
                {
                    m_pimpl->writeln_node("don't know how to handle: dropping " + peerid);
                    psk->send(peerid, beltpp::isocket_drop());

                    if (psk == m_pimpl->m_ptr_p2p_socket.get())
                        m_pimpl->remove_peer(peerid);
                    break;
                }
                }   // switch ref_packet.type()
            }
            catch (meshpp::exception_public_key const& e)
            {
                InvalidPublicKey msg;
                msg.public_key = e.pub_key;
                psk->send(peerid, msg);
                throw;
            }
            catch (meshpp::exception_private_key const& e)
            {
                InvalidPrivateKey msg;
                msg.private_key = e.priv_key;
                psk->send(peerid, msg);
                throw;
            }
            catch (meshpp::exception_signature const& e)
            {
                InvalidSignature msg;
                msg.details.public_key = e.sgn.pb_key.to_string();
                msg.details.signature = e.sgn.base58;
                BlockchainMessage::detail::loader(msg.details.package,
                                                  std::string(e.sgn.message.begin(), e.sgn.message.end()),
                                                  nullptr);

                psk->send(peerid, msg);
                throw;
            }
            catch (wrong_data_exception const&)
            {
                psk->send(peerid, beltpp::isocket_drop());
                m_pimpl->remove_peer(peerid);
                throw;
            }
            catch (wrong_request_exception const& e)
            {
                RemoteError remote_error;
                remote_error.message = e.message;
                psk->send(peerid, remote_error);
                throw;
            }
            catch (authority_exception const& e)
            {
                InvalidAuthority msg;
                msg.authority_provided = e.authority_provided;
                msg.authority_required = e.authority_required;
                psk->send(peerid, msg);
                throw;
            }
            catch (std::exception const& e)
            {
                if (it == interface_type::rpc)
                {
                    RemoteError msg;
                    msg.message = e.what();
                    psk->send(peerid, msg);
                }
                throw;
            }
            catch (...)
            {
                RemoteError msg;
                msg.message = "unknown exception";
                psk->send(peerid, msg);
                throw;
            }
            }   // for (auto& received_packet : received_packets)
        }   // for (auto& pevent_item : wait_sockets)
    }
    else if (beltpp::event_handler::timer_out == wait_result)
    {
        m_pimpl->writeln_node("timer " + std::to_string(++m_pimpl->timer_count));

        m_pimpl->m_ptr_p2p_socket->timer_action();
        m_pimpl->m_ptr_rpc_socket->timer_action();

        auto const& peerids_to_remove = m_pimpl->do_step();
        for (auto const& peerid_to_remove : peerids_to_remove)
        {
            m_pimpl->writeln_node("not answering: dropping " + peerid_to_remove);
            m_pimpl->m_ptr_p2p_socket->send(peerid_to_remove, beltpp::isocket_drop());
            m_pimpl->remove_peer(peerid_to_remove);
        }
    }

    if (m_pimpl->m_check_timer.expired())
    {
        m_pimpl->m_check_timer.update();

        if (m_pimpl->sync_peerid.empty())
        {
            if (!m_pimpl->sync_responses.empty())
            {
                // process collected SyncResponse data
                BlockHeader block_header;
                m_pimpl->m_blockchain.header(block_header);

                uint64_t block_number = block_header.block_number;
                uint64_t consensus_sum = block_header.c_sum;
                beltpp::isocket::peer_id tmp_peer;

                // if node is possible miner it should
                // mine forst current block then try to sync
                uint64_t n = m_pimpl->m_miner ? 1 : 0;

                for (auto& it : m_pimpl->sync_responses)
                {
                    if (m_pimpl->m_p2p_peers.find(it.first) == m_pimpl->m_p2p_peers.end())
                        continue; // for the case if peer is droped before sync started

                    if (block_number + n < it.second.block_number ||
                        (block_number == it.second.block_number &&
                            consensus_sum < it.second.consensus_sum))
                    {
                        block_number = it.second.block_number;
                        consensus_sum = it.second.consensus_sum;
                        tmp_peer = it.first;
                    }
                }

                m_pimpl->sync_responses.clear();

                if (!tmp_peer.empty())
                {
                    m_pimpl->sync_peerid = tmp_peer;

                    // request better chain
                    BlockHeaderRequest header_request;
                    header_request.blocks_from = block_number;
                    header_request.blocks_to = block_header.block_number;

                    beltpp::isocket* psk = m_pimpl->m_ptr_p2p_socket.get();

                    psk->send(tmp_peer, header_request);
                    m_pimpl->update_sync_time();
                    m_pimpl->reset_stored_request(tmp_peer);
                    m_pimpl->store_request(tmp_peer, header_request);
                }
            }
            else
            {
                BlockHeader header;
                m_pimpl->m_blockchain.header(header);

                system_clock::time_point current_time_point = system_clock::now();
                system_clock::time_point previous_time_point = system_clock::from_time_t(header.sign_time.tm);
                chrono::seconds diff_seconds = chrono::duration_cast<chrono::seconds>(current_time_point - previous_time_point);

                if (diff_seconds.count() >= BLOCK_MINE_DELAY && m_pimpl->timer_count > 2)
                {
                    coin amount = m_pimpl->m_state.get_balance(m_pimpl->private_key.get_public_key().to_string());

                    if (amount >= MINE_AMOUNT_THRESHOLD)
                    {
                        mine_block(m_pimpl);
                        m_pimpl->m_miner = true;
                    }
                }

                if (m_pimpl->m_sync_timer.expired())
                    m_pimpl->new_sync_request();
            }
        }
        else if (m_pimpl->sync_timeout()) // sync process step takes too long time
        {
            beltpp::isocket* psk = m_pimpl->m_ptr_p2p_socket.get();

            psk->send(m_pimpl->sync_peerid, beltpp::isocket_drop());

            m_pimpl->writeln_node("Sync node is not answering: dropping " + m_pimpl->sync_peerid);
            m_pimpl->remove_peer(m_pimpl->sync_peerid);

            m_pimpl->new_sync_request();
        }
    }

    return code;
}

}


