#include "node.hpp"
#include "node_internals.hpp"

#include "communication_rpc.hpp"
#include "communication_p2p.hpp"

#include "open_container_packet.hpp"

#include <utility>
#include <exception>
#include <string>
#include <memory>
#include <chrono>
#include <unordered_set>
#include <unordered_map>
#include <vector>

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
           meshpp::private_key pv_key)
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
                                         pv_key))
{

}

node::node(node&&) = default;

node::~node()
{

}

/*void node::send(peer_id const& peer,
                packet&& pack)
{
    Other wrapper;
    wrapper.contents = std::move(pack);
    m_pimpl->m_ptr_p2p_socket->send(peer, std::move(wrapper));
}*/

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

    //m_pimpl->writeln_node("eh.wait");
    auto wait_result = m_pimpl->m_ptr_eh->wait(wait_sockets);
    //m_pimpl->writeln_node("eh.wait - done");

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
                if (it == interface_type::p2p)
                    return peerid.substr(0, 5);
                else
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
                case Join::rtt:
                {
                    m_pimpl->write_node(str_peerid(peerid));
                    m_pimpl->writeln_node("joined");

                    if (psk == m_pimpl->m_ptr_p2p_socket.get())
                    {
                        beltpp::on_failure guard(
                            [&peerid, &psk] { psk->send(peerid, Drop()); });

                        m_pimpl->add_peer(peerid);

                        guard.dismiss();

                        m_pimpl->store_request(peerid, ChainInfoRequest());
                        psk->send(peerid, ChainInfoRequest());
                    }

                    break;
                }
                case Drop::rtt:
                {
                    m_pimpl->write_node(str_peerid(peerid));
                    m_pimpl->writeln_node("dropped");

                    if (psk == m_pimpl->m_ptr_p2p_socket.get())
                        m_pimpl->remove_peer(peerid);

                    break;
                }
                case Error::rtt:
                {
                    m_pimpl->write_node(str_peerid(peerid));
                    m_pimpl->writeln_node("error");
                    psk->send(peerid, Drop());

                    if (psk == m_pimpl->m_ptr_p2p_socket.get())
                        m_pimpl->remove_peer(peerid);

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
                        throw std::runtime_error("will process only \"broadcast signed transfer\"");
                
                    process_transfer(*broadcast_transaction.items[1],
                                     ref_packet,
                                     m_pimpl->m_action_log,
                                     m_pimpl->m_transaction_pool,
                                     m_pimpl->m_state);


                    broadcast(received_packet,
                              m_pimpl->m_ptr_p2p_socket->name(),
                              peerid,
                              (it == interface_type::rpc),
                              m_pimpl->plogger_node,
                              m_pimpl->m_p2p_peers,
                              m_pimpl->m_ptr_p2p_socket.get());
                
                    psk->send(peerid, Done());
                    break;
                }
                case ChainInfoRequest::rtt:
                {
                    ChainInfo chaininfo_msg;
                    chaininfo_msg.length = m_pimpl->m_blockchain.length();
                    psk->send(peerid, chaininfo_msg);
                    break;
                }
                case ChainInfo::rtt:
                {
                    if (it == interface_type::p2p)
                    {
                        m_pimpl->reset_stored_request(peerid);
                        if (stored_packet.type() != ChainInfoRequest::rtt)
                            throw std::runtime_error("I didn't ask for chain info");
                    }
                    break;
                }
                case LogTransaction::rtt:
                {
                    if (it == interface_type::rpc)
                        submit_action(std::move(ref_packet),
                                      m_pimpl->m_action_log,
                                      m_pimpl->m_transaction_pool,
                                      *psk,
                                      peerid);
                    break;
                }
                case RevertLastLoggedAction::rtt:
                {
                    if (it == interface_type::rpc)
                    {
                        m_pimpl->m_action_log.revert();
                        psk->send(peerid, Done());
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
                    if (it == interface_type::p2p)
                    {
                        process_sync_request(ref_packet, m_pimpl, *psk, peerid);
                    }
                    else
                    {
                        RemoteError remote_error;
                        remote_error.message = "Wrong request!";
                        psk->send(peerid, remote_error);
                    }
                    break;
                }
                case SyncResponse::rtt:
                {
                    if (it == interface_type::p2p)
                    {
                        m_pimpl->reset_stored_request(peerid);
                        if (stored_packet.type() != SyncRequest::rtt)
                        {
                            psk->send(peerid, Drop());
                            m_pimpl->remove_peer(peerid);
                            break;
                        }

                        SyncResponse sync_response;
                        std::move(ref_packet).get(sync_response);
                        m_pimpl->sync_vector.push_back(std::pair<beltpp::isocket::peer_id, SyncResponse>(peerid, sync_response));
                    }
                    else
                    {
                        RemoteError remote_error;
                        remote_error.message = "Wrong request!";
                        psk->send(peerid, remote_error);
                    }
                    break;
                }
                case ConsensusRequest::rtt:
                {
                    if (it == interface_type::p2p)
                    {
                        process_consensus_request(ref_packet, m_pimpl, *psk, peerid);
                    }
                    else
                    {
                        RemoteError remote_error;
                        remote_error.message = "Wrong request!";
                        psk->send(peerid, remote_error);
                    }
                    break;
                }
                case ConsensusResponse::rtt:
                {
                    if (it == interface_type::p2p)
                    {
                        m_pimpl->reset_stored_request(peerid);
                        if (stored_packet.type() != ConsensusRequest::rtt)
                        {
                            psk->send(peerid, Drop());
                            m_pimpl->remove_peer(peerid);
                            break;
                        }

                        process_consensus_response(ref_packet, m_pimpl);
                    }
                    else
                    {
                        RemoteError remote_error;
                        remote_error.message = "Wrong request!";
                        psk->send(peerid, remote_error);
                    }
                    break;
                }
                case BlockHeaderRequest::rtt:
                {
                    if (it == interface_type::p2p)
                    {
                        process_blockheader_request(ref_packet, m_pimpl, *psk, peerid);
                    }
                    else
                    {
                        RemoteError remote_error;
                        remote_error.message = "Wrong request!";
                        psk->send(peerid, remote_error);
                    }
                    break;
                }
                case BlockHeaderResponse::rtt:
                {
                    if (it == interface_type::p2p)
                    {
                        m_pimpl->reset_stored_request(peerid);
                        if (stored_packet.type() != BlockHeaderRequest::rtt)
                        {
                            psk->send(peerid, Drop());
                            m_pimpl->remove_peer(peerid);
                            break;
                        }

                        process_blockheader_response(ref_packet, m_pimpl, *psk, peerid);
                    }
                    else
                    {
                        RemoteError remote_error;
                        remote_error.message = "Wrong request!";
                        psk->send(peerid, remote_error);
                    }
                    break;
                }
                case BlockChainRequest::rtt:
                {
                    if (it == interface_type::p2p)
                    {
                        process_blockchain_request(ref_packet, m_pimpl, *psk, peerid);
                    }
                    else
                    {
                        RemoteError remote_error;
                        remote_error.message = "Wrong request!";
                        psk->send(peerid, remote_error);
                    }
                    break;
                }
                case BlockChainResponse::rtt:
                {
                    if (it == interface_type::p2p)
                    {
                        m_pimpl->reset_stored_request(peerid);
                        if (stored_packet.type() != BlockChainRequest::rtt)
                        {
                            psk->send(peerid, Drop());
                            m_pimpl->remove_peer(peerid);
                            break;
                        }
                        
                        process_blockchain_response(ref_packet, m_pimpl, *psk, peerid);
                    }
                    else
                    {
                        RemoteError remote_error;
                        remote_error.message = "Wrong request!";
                        psk->send(peerid, remote_error);
                    }
                    break;
                }
                default:
                {
                    m_pimpl->writeln_node("don't know how to handle: dropping " + peerid);
                    psk->send(peerid, Drop());

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
                msg.details.signature = e.sgn.base64;
                BlockchainMessage::detail::loader(msg.details.package,
                                                  std::string(e.sgn.message.begin(), e.sgn.message.end()),
                                                  nullptr);

                psk->send(peerid, msg);
                throw;
            }
            catch (exception_authority const& e)
            {
                InvalidAuthority msg;
                msg.authority_provided = e.authority_provided;
                msg.authority_required = e.authority_required;
                psk->send(peerid, msg);
                throw;
            }
            catch(std::exception const& e)
            {
                RemoteError msg;
                msg.message = e.what();
                psk->send(peerid, msg);
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
        m_pimpl->writeln_node("timer");

        m_pimpl->m_ptr_p2p_socket->timer_action();
        m_pimpl->m_ptr_rpc_socket->timer_action();

        auto const& peerids_to_remove = m_pimpl->do_step();
        for (auto const& peerid_to_remove : peerids_to_remove)
        {
            m_pimpl->writeln_node("not answering: dropping " + peerid_to_remove);
            m_pimpl->m_ptr_p2p_socket->send(peerid_to_remove, Drop());
            m_pimpl->remove_peer(peerid_to_remove);
        }

        beltpp::isocket* psk = m_pimpl->m_ptr_p2p_socket.get();

        // there is no ongoing sync process
        if (m_pimpl->header_vector.empty())
        {
            // Sync node
            // process collected SyncResponse data
            BlockHeader block_header;
            m_pimpl->m_blockchain.header(block_header);

            SyncRequest sync_request;
            sync_request.block_number = block_header.block_number;
            sync_request.consensus_sum = block_header.consensus_sum;
            beltpp::isocket::peer_id tmp_peer = "tmp_peer";

            for (auto& it : m_pimpl->sync_vector)
            {
                if (sync_request.block_number < it.second.block_number ||
                    (sync_request.block_number == it.second.block_number &&
                        sync_request.consensus_sum < it.second.consensus_sum))
                {
                    sync_request.block_number = it.second.block_number;
                    sync_request.consensus_sum = it.second.consensus_sum;
                    tmp_peer = it.first;
                }
            }

            m_pimpl->sync_vector.clear();

            if (tmp_peer != "tmp_peer")
            {
                // request better chain
                BlockHeaderRequest header_request;
                header_request.blocks_from = sync_request.block_number;
                header_request.blocks_to = block_header.block_number;

                psk->send(tmp_peer, header_request);
                m_pimpl->reset_stored_request(tmp_peer);
                m_pimpl->store_request(tmp_peer, header_request);
            }
            else
            {
                // new sync request 
                sync_request.block_number = block_header.block_number;
                sync_request.consensus_sum = block_header.consensus_sum;

                for (auto& it : m_pimpl->m_p2p_peers)
                {
                    psk->send(it, sync_request);
                    m_pimpl->reset_stored_request(it);
                    m_pimpl->store_request(it, sync_request);
                }
            }

            // Mine block
            uint64_t amount = m_pimpl->m_state.get_balance(m_pimpl->private_key.get_public_key().to_string());

            if (m_pimpl->m_blockchain.mine_block(m_pimpl->private_key, amount, m_pimpl->m_transaction_pool))
            {
                BlockHeader tmp_header;

                if (m_pimpl->m_blockchain.tmp_header(tmp_header))
                {
                    ConsensusRequest consensus_request;
                    consensus_request.block_number = tmp_header.block_number;
                    consensus_request.consensus_delta = tmp_header.consensus_delta;

                    for (auto& it : m_pimpl->m_p2p_peers)
                    {
                        psk->send(it, consensus_request);
                        m_pimpl->reset_stored_request(it);
                        m_pimpl->store_request(it, consensus_request);
                    }
                }
            }

            // Apply own block
            m_pimpl->m_blockchain.step_apply();
        }
    }

    return code;
}

}


