#include "node.hpp"
#include "common.hpp"

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
#include <thread>

using namespace BlockchainMessage;

using beltpp::ip_address;
using beltpp::socket;
using beltpp::packet;
using peer_id = socket::peer_id;

namespace chrono = std::chrono;
using chrono::system_clock;

using std::pair;
using std::string;
using std::vector;
using std::unordered_set;

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
           node_type& n_type,
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
                                         n_type,
                                         log_enabled))
{

}

node::node(node&&) noexcept = default;

node::~node() = default;

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

    unordered_set<beltpp::ievent_item const*> wait_sockets;

    auto wait_result = m_pimpl->m_ptr_eh->wait(wait_sockets);

    enum class interface_type {p2p, rpc};

    if (wait_result == beltpp::event_handler::event)
    {
        for (auto& pevent_item : wait_sockets)
        {
            interface_type it = interface_type::rpc;
            if (pevent_item == &m_pimpl->m_ptr_p2p_socket->worker())
                it = interface_type::p2p;

            auto str_receive = [it]
            {
                if (it == interface_type::p2p)
                    return "p2p_sk.receive";
                return "rpc_sk.receive";
            };
            str_receive();

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

                open_container_packet<Broadcast, SignedTransaction> broadcast_signed_transaction;
                open_container_packet<Broadcast> broadcast_anything;
                bool is_container = broadcast_signed_transaction.open(received_packet, composition) ||
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
                    m_pimpl->writeln_node("joined: " + detail::peer_short_names(peerid));

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
                    m_pimpl->writeln_node("dropped: " + detail::peer_short_names(peerid));

                    if (psk == m_pimpl->m_ptr_p2p_socket.get())
                        m_pimpl->remove_peer(peerid);

                    break;
                }
                case beltpp::isocket_protocol_error::rtt:
                {
                    beltpp::isocket_protocol_error msg;
                    ref_packet.get(msg);
                    m_pimpl->writeln_node("protocol error: " + detail::peer_short_names(peerid));
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
                    m_pimpl->writeln_node_warning(msg.reason + ", " + peerid);
                    break;
                }
                case beltpp::isocket_open_error::rtt:
                {
                    beltpp::isocket_open_error msg;
                    ref_packet.get(msg);
                    m_pimpl->writeln_node_warning(msg.reason + ", " + peerid);
                    break;
                }
                case Transfer::rtt:
                {
                    if (broadcast_signed_transaction.items.empty())
                        throw std::runtime_error("will process only \"broadcast signed transaction\"");

                    Broadcast* p_broadcast = nullptr;
                    SignedTransaction* p_signed_tx = nullptr;
                    Transfer* p_transfer = nullptr;

                    broadcast_signed_transaction.items[0]->get(p_broadcast);
                    broadcast_signed_transaction.items[1]->get(p_signed_tx);
                    ref_packet.get(p_transfer);

                    assert(p_broadcast);
                    assert(p_signed_tx);
                    assert(p_transfer);

                    Broadcast& broadcast = *p_broadcast;
                    SignedTransaction& signed_tx = *p_signed_tx;
                    Transfer& transfer = *p_transfer;
                
                    if(process_transfer(signed_tx, transfer, m_pimpl))
                        process_broadcast(std::move(broadcast),
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
                case Contract::rtt:
                {
                    if (it == interface_type::rpc)
                        throw std::runtime_error("request restricted for rpc interface");

                    if (broadcast_signed_transaction.items.empty())
                        throw std::runtime_error("will process only \"broadcast signed transaction\"");

                    Broadcast* p_broadcast = nullptr;
                    SignedTransaction* p_signed_tx = nullptr;
                    Contract* p_contract = nullptr;

                    broadcast_signed_transaction.items[0]->get(p_broadcast);
                    broadcast_signed_transaction.items[1]->get(p_signed_tx);
                    ref_packet.get(p_contract);

                    assert(p_broadcast);
                    assert(p_signed_tx);
                    assert(p_contract);

                    Broadcast& broadcast = *p_broadcast;
                    SignedTransaction& signed_tx = *p_signed_tx;
                    Contract& contract = *p_contract;

                    if (m_pimpl->m_state.get_contract_type(contract.owner) == 
                        publiqpp::detail::node_type_to_int(publiqpp::node_type::unknown) &&
                        process_contract(signed_tx, contract, m_pimpl))
                    {
                        process_broadcast(std::move(broadcast),
                                          m_pimpl->m_ptr_p2p_socket->name(),
                                          peerid,
                                          false,
                                          nullptr,
                                          m_pimpl->m_p2p_peers,
                                          m_pimpl->m_ptr_p2p_socket.get());
                    }

                    break;
                }
                case StatInfo::rtt:
                {
                    if (it == interface_type::rpc)
                        throw std::runtime_error("request restricted for rpc interface");

                    if (broadcast_signed_transaction.items.empty())
                        throw std::runtime_error("will process only \"broadcast signed transaction\"");

                    Broadcast* p_broadcast = nullptr;
                    SignedTransaction* p_signed_tx = nullptr;
                    StatInfo* p_stat_info = nullptr;

                    broadcast_signed_transaction.items[0]->get(p_broadcast);
                    broadcast_signed_transaction.items[1]->get(p_signed_tx);
                    ref_packet.get(p_stat_info);

                    assert(p_broadcast);
                    assert(p_signed_tx);
                    assert(p_stat_info);

                    Broadcast& broadcast = *p_broadcast;
                    SignedTransaction& signed_tx = *p_signed_tx;
                    StatInfo& stat_info = *p_stat_info;

                    m_pimpl->writeln_node("StatInfo broadcast received -> " + stat_info.to_string());

                    if (process_stat_info(signed_tx, m_pimpl))
                    {
                        process_broadcast(std::move(broadcast),
                                          m_pimpl->m_ptr_p2p_socket->name(),
                                          peerid,
                                          false,
                                          nullptr,
                                          m_pimpl->m_p2p_peers,
                                          m_pimpl->m_ptr_p2p_socket.get());
                    }

                    break;
                }
                case StorageFile::rtt:
                {
                    StorageFile file;
                    std::move(ref_packet).get(file);
                    StorageFileAddress addr;
                    //  block file upload functionality for now
                    //addr.uri = m_pimpl->m_storage.put(std::move(file));
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
                    {
                        LoggedTransactionsRequest msg_get_actions;
                        std::move(ref_packet).get(msg_get_actions);
                        get_actions(msg_get_actions, m_pimpl->m_action_log, *psk, peerid);
                    }
                    break;
                }
                case DigestRequest::rtt:
                {
                    DigestRequest msg_get_hash;
                    std::move(ref_packet).get(msg_get_hash);
                    get_hash(std::move(msg_get_hash), *psk, peerid);
                    break;
                }
                case MasterKeyRequest::rtt:
                {
                    get_random_seed(*psk, peerid);
                    break;
                }
                case KeyPairRequest::rtt:
                {
                    KeyPairRequest kpr_msg;
                    std::move(ref_packet).get(kpr_msg);
                    get_key_pair(kpr_msg, *psk, peerid);
                    break;
                }
                case SignRequest::rtt:
                {
                    SignRequest msg_sign_request;
                    std::move(ref_packet).get(msg_sign_request);
                    get_signature(std::move(msg_sign_request), *psk, peerid);
                    break;
                }
                case Signature::rtt:
                {
                    Signature msg_signature;
                    std::move(ref_packet).get(msg_signature);
                    verify_signature(msg_signature, *psk, peerid);
                    break;
                }
                case SyncRequest::rtt:
                {
                    if (it != interface_type::p2p)
                        throw wrong_request_exception("SyncRequest received through rpc!");

                    BlockHeader header;
                    m_pimpl->m_blockchain.header(header);

                    SyncResponse sync_response;
                    sync_response.number = header.block_number;
                    sync_response.c_sum = header.c_sum;

                    if (m_pimpl->net_sync_info.c_sum > m_pimpl->own_sync_info.c_sum)
                        sync_response.sync_info = m_pimpl->net_sync_info;
                    else
                        sync_response.sync_info = m_pimpl->own_sync_info;

                    psk->send(peerid, std::move(sync_response));

                    break;
                }
                case SyncResponse::rtt:
                {
                    if (it != interface_type::p2p)
                        throw wrong_request_exception("SyncResponse received through rpc!");

                    m_pimpl->reset_stored_request(peerid);
                    if (stored_packet.type() != SyncRequest::rtt)
                        throw wrong_data_exception("SyncResponse");

                    SyncResponse sync_response;
                    std::move(ref_packet).get(sync_response);

                    SyncInfo sync_info;
                    std::move(sync_response.sync_info).get(sync_info);

                    if (sync_info.number == m_pimpl->own_sync_info.number &&
                        sync_info.c_sum > m_pimpl->net_sync_info.c_sum)
                    {
                        m_pimpl->net_sync_info = sync_info;

                        if (m_pimpl->net_sync_info.c_sum > m_pimpl->own_sync_info.c_sum)
                            m_pimpl->writeln_node("Next block waiting from " +
                                                  detail::peer_short_names(m_pimpl->net_sync_info.authority));
                    }

                    m_pimpl->sync_responses.push_back(std::pair<beltpp::isocket::peer_id, SyncResponse>(peerid, sync_response));

                    break;
                }
                case BlockHeaderRequest::rtt:
                {
                    if (it != interface_type::p2p)
                        throw wrong_request_exception("BlockHeaderRequest received through rpc!");

                    BlockHeaderRequest header_request;
                    std::move(ref_packet).get(header_request);

                    process_blockheader_request(header_request, m_pimpl, *psk, peerid);

                    break;
                }
                case BlockHeaderResponse::rtt:
                {
                    if (it != interface_type::p2p)
                        throw wrong_request_exception("BlockHeaderResponse received through rpc!");

                    m_pimpl->reset_stored_request(peerid);
                    if (stored_packet.type() != BlockHeaderRequest::rtt)
                        throw wrong_data_exception("BlockHeaderResponse");

                    BlockHeaderResponse header_response;
                    std::move(ref_packet).get(header_response);

                    if(m_pimpl->sync_peerid == peerid)
                        process_blockheader_response(std::move(header_response), m_pimpl, *psk, peerid);

                    break;
                }
                case BlockchainRequest::rtt:
                {
                    if (it != interface_type::p2p)
                        throw wrong_request_exception("BlockChainRequest received through rpc!");

                    BlockchainRequest blockchain_request;
                    std::move(ref_packet).get(blockchain_request);

                    process_blockchain_request(blockchain_request, m_pimpl, *psk, peerid);

                    break;
                }
                case BlockchainResponse::rtt:
                {
                    if (it != interface_type::p2p)
                        throw wrong_request_exception("BlockChainResponse received through rpc!");

                    m_pimpl->reset_stored_request(peerid);
                    if (stored_packet.type() != BlockchainRequest::rtt)
                        throw wrong_data_exception("BlockChainResponse");

                    BlockchainResponse blockchain_response;
                    std::move(ref_packet).get(blockchain_response);

                    bool have_signed_blocks = (false == blockchain_response.signed_blocks.empty());
                    if (have_signed_blocks)
                    {
                        uint64_t temp_from = 0;
                        uint64_t temp_to = 0;

                        auto const& front = blockchain_response.signed_blocks.front().block_details;
                        auto const& back = blockchain_response.signed_blocks.back().block_details;

                        temp_from = front.header.block_number;
                        temp_to = back.header.block_number;

                        if(temp_from == temp_to)
                            m_pimpl->writeln_node("proc. block " + std::to_string(temp_from) + 
                                                  " from " + detail::peer_short_names(peerid));
                        else
                            m_pimpl->writeln_node("proc. blocks [" + std::to_string(temp_from) + 
                                                  "," + std::to_string(temp_to) + "] from " + detail::peer_short_names(peerid));
                    }

                    if (m_pimpl->sync_peerid == peerid) //  is it an error in "else" case?
                        process_blockchain_response(std::move(blockchain_response), m_pimpl, *psk, peerid);

                    if (have_signed_blocks)
                        m_pimpl->writeln_node("    done");

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

    // test ! print summary report about connections
    if (m_pimpl->m_summary_report_timer.expired())
    {
        m_pimpl->m_summary_report_timer.update();

        m_pimpl->writeln_node("Summary Report");
        m_pimpl->writeln_node("    p2p nodes connected");
        if (m_pimpl->m_p2p_peers.empty())
            m_pimpl->writeln_node("        none");
        else
        {
            for (auto const& item : m_pimpl->m_p2p_peers)
                m_pimpl->writeln_node("        " + detail::peer_short_names(item));
        }
        m_pimpl->writeln_node("    blockchain heigth: " +
                              std::to_string(m_pimpl->m_blockchain.length()));
        m_pimpl->writeln_node("End Summary Report");
    }

    // broadcast own transactions to all peers for the case
    // if node could not do this when received it through rpc
    if (m_pimpl->m_broadcast_timer.expired() && !m_pimpl->m_p2p_peers.empty())
    {
        m_pimpl->m_broadcast_timer.update();

        vector<string> pool_keys;
        m_pimpl->m_transaction_pool.get_keys(pool_keys);

        if (!pool_keys.empty())
        {
            m_pimpl->writeln_node("broadcasting " + std::to_string(pool_keys.size()) + " stored transactions to all peers");

            for (auto& key : pool_keys)
            {
                SignedTransaction signed_transaction;
                m_pimpl->m_transaction_pool.at(key, signed_transaction);

                Broadcast broadcast;
                broadcast.echoes = 2;
                broadcast.package = signed_transaction;

                process_broadcast(std::move(broadcast),
                                  m_pimpl->m_ptr_p2p_socket->name(),
                                  m_pimpl->m_ptr_p2p_socket->name(),
                                  true, // like from rpc
                                  nullptr, // no logger
                                  m_pimpl->m_p2p_peers,
                                  m_pimpl->m_ptr_p2p_socket.get());

                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            m_pimpl->writeln_node("broadcast done");
        }
    }

    // clean old transaction keys from cache
    // to minimize it and make it work faster
    if (m_pimpl->m_cache_cleanup_timer.expired())
    {
        m_pimpl->m_cache_cleanup_timer.update();

        m_pimpl->clean_transaction_cache();

        // temp place
        m_pimpl->clean_stat_cache();
        broadcast_node_type(m_pimpl);
    }

    // init sync process and block mining
    if (m_pimpl->m_check_timer.expired())
    {
        m_pimpl->m_check_timer.update();

        if (m_pimpl->sync_peerid.empty())
        {
            bool sync_now = false;

            BlockHeader header;
            m_pimpl->m_blockchain.header(header);

            system_clock::time_point current_time_point = system_clock::now();
            system_clock::time_point previous_time_point = system_clock::from_time_t(header.time_signed.tm);
            chrono::seconds diff_seconds = chrono::duration_cast<chrono::seconds>(current_time_point - previous_time_point);

            if (!m_pimpl->sync_responses.empty())
            {
                // process collected SyncResponse data
                uint64_t block_number = header.block_number;
                uint64_t consensus_sum = header.c_sum;
                beltpp::isocket::peer_id tmp_peer;

                for (auto& it : m_pimpl->sync_responses)
                {
                    if (m_pimpl->m_p2p_peers.find(it.first) == m_pimpl->m_p2p_peers.end())
                        continue; // for the case if peer is droped before sync started

                    if (block_number < it.second.number ||
                        (block_number == it.second.number && consensus_sum < it.second.c_sum))
                    {
                        block_number = it.second.number;
                        consensus_sum = it.second.c_sum;
                        tmp_peer = it.first;
                    }
                }

                sync_now = block_number != header.block_number + 1;

                // sync candidate has the next block number
                if (!sync_now)
                {
                    if (consensus_sum > std::max(m_pimpl->own_sync_info.c_sum, m_pimpl->net_sync_info.c_sum))
                        sync_now = true; // suddenly got something better than expected
                    else if (m_pimpl->own_sync_info.c_sum > consensus_sum)
                        m_pimpl->sync_responses.clear(); // I can mine better block and dont need received data
                    else
                    {
                        if (consensus_sum == m_pimpl->net_sync_info.c_sum)
                            sync_now = true; // got expected block
                        else if (diff_seconds.count() > BLOCK_MINE_DELAY + BLOCK_WAIT_DELAY)
                            sync_now = true; // it is too late and network has better block than I can mine
                    }                                            
                }

                if (sync_now)
                {
                    m_pimpl->sync_responses.clear();

                    if (!tmp_peer.empty())
                    {
                        m_pimpl->sync_peerid = tmp_peer;

                        // request better chain
                        BlockHeaderRequest header_request;
                        header_request.blocks_from = block_number;
                        header_request.blocks_to = header.block_number;

                        beltpp::isocket* psk = m_pimpl->m_ptr_p2p_socket.get();

                        psk->send(tmp_peer, header_request);
                        m_pimpl->update_sync_time();
                        m_pimpl->reset_stored_request(tmp_peer);
                        m_pimpl->store_request(tmp_peer, header_request);
                    }
                }
            }
            
            if (!sync_now && m_pimpl->sync_responses.empty())
            {
                if (m_pimpl->m_miner && diff_seconds.count() >= BLOCK_MINE_DELAY &&
                    (m_pimpl->own_sync_info.c_sum >= m_pimpl->net_sync_info.c_sum ||
                        diff_seconds.count() > BLOCK_MINE_DELAY + BLOCK_WAIT_DELAY))
                    mine_block(m_pimpl);

                if (m_pimpl->m_sync_timer.expired())
                    m_pimpl->new_sync_request();
            }
        }
        else if (m_pimpl->sync_timeout()) // sync process step takes too long time
        {
            beltpp::isocket* psk = m_pimpl->m_ptr_p2p_socket.get();

            psk->send(m_pimpl->sync_peerid, beltpp::isocket_drop());

            m_pimpl->writeln_node("Sync node is not answering: dropping " +
                                  detail::peer_short_names(m_pimpl->sync_peerid));
            m_pimpl->remove_peer(m_pimpl->sync_peerid);

            m_pimpl->new_sync_request();
        }
    }

    return code;
}

}


