#include "node.hpp"
#include "common.hpp"
#include "exception.hpp"

#include "communication_rpc.hpp"
#include "communication_p2p.hpp"
#include "transaction_handler.hpp"

#include "open_container_packet.hpp"
#include "sessions.hpp"

#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <exception>

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
using std::unique_ptr;

namespace publiqpp
{

/*
 * node
 */
node::node(string const& genesis_signed_block,
           ip_address const & public_address,
           ip_address const& rpc_bind_to_address,
           ip_address const& slave_connect_to_address,
           ip_address const& p2p_bind_to_address,
           std::vector<ip_address> const& p2p_connect_to_addresses,
           boost::filesystem::path const& fs_blockchain,
           boost::filesystem::path const& fs_action_log,
           boost::filesystem::path const& fs_transaction_pool,
           boost::filesystem::path const& fs_state,
           boost::filesystem::path const& fs_documents,
           beltpp::ilog* plogger_p2p,
           beltpp::ilog* plogger_node,
           meshpp::private_key const& pv_key,
           NodeType& n_type,
           bool log_enabled,
           bool transfer_only)
    : m_pimpl(new detail::node_internals(genesis_signed_block,
                                         public_address,
                                         rpc_bind_to_address,
                                         slave_connect_to_address,
                                         p2p_bind_to_address,
                                         p2p_connect_to_addresses,
                                         fs_blockchain,
                                         fs_action_log,
                                         fs_transaction_pool,
                                         fs_state,
                                         fs_documents,
                                         plogger_p2p,
                                         plogger_node,
                                         pv_key,
                                         n_type,
                                         log_enabled,
                                         transfer_only))
{}

node::node(node&&) noexcept = default;

node::~node() = default;

void node::wake()
{
    m_pimpl->m_ptr_eh->wake();
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

    if (wait_result & beltpp::event_handler::event)
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

            if (nullptr == psk)
                throw std::logic_error("event handler behavior");

            beltpp::socket::packets received_packets;
            if (psk != nullptr)
                received_packets = psk->receive(peerid);

            //-----------------------------------------------------//
            auto remove_peer = [&]()
            {
                if (psk == m_pimpl->m_ptr_p2p_socket.get())
                {
                    m_pimpl->remove_peer(peerid);
                    m_pimpl->m_nodeid_sessions.remove(peerid);
                }
                else if (peerid == m_pimpl->m_slave_peer)
                {
                    m_pimpl->m_slave_peer.clear();
                    m_pimpl->writeln_node(" <=  /  => Slave disconnected!");
                }
            };
            //-----------------------------------------------------//


            for (auto& received_packet : received_packets)
            {
            try
            {
                if (m_pimpl->m_nodeid_sessions.process(peerid, std::move(received_packet)))
                    continue;

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

                switch (ref_packet.type())
                {
                case beltpp::isocket_join::rtt:
                {
                    if (peerid != m_pimpl->m_slave_peer_attempt)
                        m_pimpl->writeln_node("joined: " + detail::peer_short_names(peerid));

                    if (psk == m_pimpl->m_ptr_p2p_socket.get())
                    {
                        beltpp::on_failure guard(
                            [&peerid, &psk] { psk->send(peerid, beltpp::packet(beltpp::isocket_drop())); });

                        m_pimpl->add_peer(peerid);

                        beltpp::ip_address external_address =
                                m_pimpl->m_ptr_p2p_socket->external_address();
                        assert(external_address.local.empty() == false);
                        assert(external_address.remote.empty());
                        external_address.local.port =
                                m_pimpl->m_rpc_bind_to_address.local.port;

                        guard.dismiss();
                    }
                    else
                    {
                        beltpp::on_failure guard(
                            [&peerid, &psk] { psk->send(peerid, beltpp::packet(beltpp::isocket_drop())); });

                        if (peerid == m_pimpl->m_slave_peer_attempt)
                        {
                            m_pimpl->m_slave_peer = peerid;
                            m_pimpl->writeln_node(" <=======> Slave connected!");
                        }

                        guard.dismiss();
                    }

                    break;
                }
                case beltpp::isocket_drop::rtt:
                {
                    m_pimpl->writeln_node("dropped: " + detail::peer_short_names(peerid));

                    remove_peer();

                    break;
                }
                case beltpp::isocket_protocol_error::rtt:
                {
                    beltpp::isocket_protocol_error msg;
                    ref_packet.get(msg);
                    m_pimpl->writeln_node("protocol error: " + detail::peer_short_names(peerid));
                    m_pimpl->writeln_node(msg.buffer);
                    psk->send(peerid, beltpp::packet(beltpp::isocket_drop()));

                    remove_peer();
                    
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
                case File::rtt:
                case ContentUnit::rtt:
                case Content::rtt:
                case Role::rtt:
                case StorageUpdate::rtt:
                case ServiceStatistics::rtt:
                {
                    if (broadcast_signed_transaction.items.empty())
                        throw wrong_data_exception("will process only \"broadcast signed transaction\"");

                    if (m_pimpl->m_transfer_only &&
                        ref_packet.type() != Transfer::rtt)
                        throw std::runtime_error("this is coin only blockchain");

                    Broadcast* p_broadcast = nullptr;
                    SignedTransaction* p_signed_tx = nullptr;

                    broadcast_signed_transaction.items[0]->get(p_broadcast);
                    broadcast_signed_transaction.items[1]->get(p_signed_tx);

                    assert(p_broadcast);
                    assert(p_signed_tx);

                    Broadcast& broadcast = *p_broadcast;
                    SignedTransaction& signed_tx = *p_signed_tx;
                
                    broadcast_type process_result;
                    process_result = action_process_on_chain(signed_tx, *m_pimpl.get());

                    if (process_result != broadcast_type::none)
                    {
                        broadcast_message(std::move(broadcast),
                                          m_pimpl->m_ptr_p2p_socket->name(),
                                          peerid,
                                          (it == interface_type::rpc ||
                                           process_result == broadcast_type::full_broadcast),
                                          //m_pimpl->plogger_node,
                                          nullptr,
                                          m_pimpl->m_p2p_peers,
                                          m_pimpl->m_ptr_p2p_socket.get());
                    }
                
                    if (it == interface_type::rpc)
                        psk->send(peerid, beltpp::packet(Done()));

                    break;
                }
                case AddressInfo::rtt:
                {
                    if (broadcast_signed_transaction.items.empty())
                        throw wrong_data_exception("will process only \"broadcast signed transaction\"");

                    if (it != interface_type::p2p)
                        throw wrong_request_exception("AddressInfo received through rpc!");

                    Broadcast* p_broadcast = nullptr;
                    SignedTransaction* p_signed_tx = nullptr;
                    AddressInfo* p_address_info = nullptr;

                    broadcast_signed_transaction.items[0]->get(p_broadcast);
                    broadcast_signed_transaction.items[1]->get(p_signed_tx);
                    ref_packet.get(p_address_info);

                    assert(p_broadcast);
                    assert(p_signed_tx);
                    assert(p_address_info);

                    Broadcast& broadcast = *p_broadcast;
                    SignedTransaction& signed_tx = *p_signed_tx;
                    AddressInfo& address_info = *p_address_info;

                    if (process_address_info(signed_tx, address_info, m_pimpl))
                    {
                        beltpp::ip_address beltpp_ip_address;
                        beltpp::assign(beltpp_ip_address, address_info.ip_address);

                        nodeid_address_info& nodeid_info = m_pimpl->m_nodeid_service.nodeids[address_info.node_address];

                        nodeid_info.add(beltpp_ip_address,
                                        unique_ptr<session_action_broadcast_address_info>(
                                            new session_action_broadcast_address_info(*m_pimpl.get(),
                                                                                      peerid,
                                                                                      std::move(broadcast))));
                    }

                    break;
                }
                case StorageFile::rtt:
                {
                    if (NodeType::blockchain == m_pimpl->m_node_type||
                        nullptr == m_pimpl->m_slave_node)
                        throw wrong_request_exception("Do not distrub!");

                    if (m_pimpl->m_slave_node)
                    {
                        StorageFile storage_file;
                        std::move(ref_packet).get(storage_file);

                        vector<unique_ptr<meshpp::session_action<meshpp::session_header>>> actions;
                        actions.emplace_back(new session_action_save_file(*m_pimpl.get(),
                                                                          std::move(storage_file),
                                                                          *psk,
                                                                          peerid));

                        meshpp::session_header header;
                        header.peerid = "slave";
                        m_pimpl->m_sessions.add(header,
                                                std::move(actions),
                                                chrono::minutes(1));
                    }
                    
                    break;
                }
                case StorageFileDelete::rtt:
                {
                    if (NodeType::blockchain == m_pimpl->m_node_type ||
                        nullptr == m_pimpl->m_slave_node)
                        throw wrong_request_exception("Do not distrub!");

                    if (m_pimpl->m_slave_node)
                    {
                        StorageFileDelete storage_file_delete;
                        std::move(ref_packet).get(storage_file_delete);

                        vector<unique_ptr<meshpp::session_action<meshpp::session_header>>> actions;
                        actions.emplace_back(new session_action_delete_file(*m_pimpl.get(),
                                                                            std::move(storage_file_delete.uri),
                                                                            *psk,
                                                                            peerid));

                        meshpp::session_header header;
                        header.peerid = "slave";
                        m_pimpl->m_sessions.add(header,
                                                std::move(actions),
                                                chrono::minutes(1));
                    }

                    break;
                }
                /*
                case TaskResponse::rtt:
                {
                    if(peerid != m_pimpl->m_slave_peer)
                        throw wrong_request_exception("Do not distrub!");

                    TaskResponse task_response;
                    std::move(ref_packet).get(task_response);

                    if (task_response.package.type() == StorageFileAddress::rtt)
                    {
                        packet task_packet;
                        m_pimpl->m_slave_tasks.remove(task_response.task_id, task_packet);
                    }
                    else if (task_response.package.type() == ServiceStatistics::rtt)
                    {
                        if (m_pimpl->m_node_type == NodeType::storage)
                        {
                            packet task_packet;

                            if (m_pimpl->m_slave_tasks.remove(task_response.task_id, task_packet) &&
                                task_packet.type() == ServiceStatistics::rtt)
                            {
                                ServiceStatistics stat_info;
                                std::move(task_response.package).get(stat_info);

                                broadcast_storage_stat(stat_info, m_pimpl);
                            }
                        }
                        else
                            throw wrong_request_exception("Do not distrub!");
                    }

                    break;
                }
                */
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

                    BlockHeaderExtended const& header = m_pimpl->m_blockchain.header_ex_at(m_pimpl->m_blockchain.length() - 1);

                    SyncResponse sync_response;
                    sync_response.own_header = header;

                    if (m_pimpl->all_sync_info.net_sync_info().c_sum >
                        m_pimpl->all_sync_info.own_sync_info().c_sum)
                        sync_response.promised_header = m_pimpl->all_sync_info.net_sync_info();
                    else
                        sync_response.promised_header = m_pimpl->all_sync_info.own_sync_info();

                    //m_pimpl->writeln_node("sync response - " + peerid);
                    psk->send(peerid, beltpp::packet(std::move(sync_response)));

                    break;
                }
                case BlockHeaderRequest::rtt:
                {
                    if (it != interface_type::p2p)
                        throw wrong_request_exception("BlockHeaderRequest received through rpc!");

                    BlockHeaderRequest header_request;
                    std::move(ref_packet).get(header_request);

                    //m_pimpl->writeln_node("header response - " + peerid);
                    session_action_header::process_request(peerid,
                                                           header_request,
                                                           *m_pimpl.get());

                    break;
                }
                case BlockchainRequest::rtt:
                {
                    if (it != interface_type::p2p)
                        throw wrong_request_exception("BlockchainRequest received through rpc!");

                    BlockchainRequest blockchain_request;
                    std::move(ref_packet).get(blockchain_request);

                    //m_pimpl->writeln_node("chain response - " + peerid);
                    session_action_block::process_request(peerid,
                                                          blockchain_request,
                                                          *m_pimpl.get());

                    break;
                }
                case TransactionBroadcastRequest::rtt:
                {
                    TransactionBroadcastRequest transaction_broadcast_request;
                    std::move(ref_packet).get(transaction_broadcast_request);

                    Authority authorization;
                    meshpp::private_key pv(transaction_broadcast_request.private_key);
                    authorization.address = pv.get_public_key().to_string();
                    authorization.signature = pv.sign(transaction_broadcast_request.to_string()).base58;

                    BlockchainMessage::SignedTransaction signed_transaction;
                    signed_transaction.transaction_details = transaction_broadcast_request.transaction_details;
                    signed_transaction.authorizations.push_back(authorization);

                    TransactionDone transaction_done;
                    transaction_done.transaction_hash = meshpp::hash(signed_transaction.to_string());

                    BlockchainMessage::Broadcast broadcast;
                    broadcast.echoes = 2;
                    broadcast.package = std::move(signed_transaction);

                    broadcast_message(std::move(broadcast),
                                      m_pimpl->m_ptr_p2p_socket->name(),
                                      peerid,
                                      true,
                                      nullptr,
                                      m_pimpl->m_p2p_peers,
                                      m_pimpl->m_ptr_p2p_socket.get());

                    psk->send(peerid, beltpp::packet(std::move(transaction_done)));

                    break;
                }
                default:
                {
                    m_pimpl->writeln_node("master don't know how to handle: " + std::to_string(ref_packet.type())/* +
                                          ". dropping " + detail::peer_short_names(peerid)*/);

                    //psk->send(peerid, beltpp::isocket_drop());
                    //
                    //remove_peer();

                    break;
                }
                }   // switch ref_packet.type()
            }
            catch (meshpp::exception_public_key const& e)
            {
                if (it == interface_type::rpc)
                {
                    InvalidPublicKey msg;
                    msg.public_key = e.pub_key;
                    psk->send(peerid, beltpp::packet(msg));
                }
                else
                {
                    psk->send(peerid, beltpp::packet(beltpp::isocket_drop()));
                    remove_peer();
                }
                throw;
            }
            catch (meshpp::exception_private_key const& e)
            {
                if (it == interface_type::rpc)
                {
                    InvalidPrivateKey msg;
                    msg.private_key = e.priv_key;
                    psk->send(peerid, beltpp::packet(msg));
                }
                else
                {
                    psk->send(peerid, beltpp::packet(beltpp::isocket_drop()));
                    remove_peer();
                }
                throw;
            }
            catch (meshpp::exception_signature const& e)
            {
                if (it == interface_type::rpc)
                {
                    InvalidSignature msg;
                    msg.details.public_key = e.sgn.pb_key.to_string();
                    msg.details.signature = e.sgn.base58;
                    BlockchainMessage::detail::loader(msg.details.package,
                                                      std::string(e.sgn.message.begin(), e.sgn.message.end()),
                                                      nullptr);

                    psk->send(peerid, beltpp::packet(msg));
                }
                else
                {
                    psk->send(peerid, beltpp::packet(beltpp::isocket_drop()));
                    remove_peer();
                }
                throw;
            }
            catch (wrong_data_exception const& e)
            {
                if (it == interface_type::rpc)
                {
                    RemoteError remote_error;
                    remote_error.message = e.message;
                    psk->send(peerid, beltpp::packet(remote_error));
                }
                else
                {
                    psk->send(peerid, beltpp::packet(beltpp::isocket_drop()));
                    remove_peer();
                }
                throw;
            }
            catch (wrong_request_exception const& e)
            {
                if (it == interface_type::rpc)
                {
                    RemoteError remote_error;
                    remote_error.message = e.message;
                    psk->send(peerid, beltpp::packet(remote_error));
                }
                else
                {
                    psk->send(peerid, beltpp::packet(beltpp::isocket_drop()));
                    remove_peer();
                }
                throw;
            }
            catch (wrong_document_exception const& e)
            {
                if (it == interface_type::rpc)
                {
                    RemoteError remote_error;
                    remote_error.message = e.message;
                    psk->send(peerid, beltpp::packet(remote_error));
                }
                else
                {
                    psk->send(peerid, beltpp::packet(beltpp::isocket_drop()));
                    remove_peer();
                }
                throw;
            }
            catch (authority_exception const& e)
            {
                if (it == interface_type::rpc)
                {
                    InvalidAuthority msg;
                    msg.authority_provided = e.authority_provided;
                    msg.authority_required = e.authority_required;
                    psk->send(peerid, beltpp::packet(msg));
                }
                else
                {
                    psk->send(peerid, beltpp::packet(beltpp::isocket_drop()));
                    remove_peer();
                }
                throw;
            }
            catch (not_enough_balance_exception const& e)
            {
                if (it == interface_type::rpc)
                {
                    NotEnoughBalance msg;
                    e.balance.to_Coin(msg.balance);
                    e.spending.to_Coin(msg.spending);
                    psk->send(peerid, beltpp::packet(msg));
                }
                else
                {
                    psk->send(peerid, beltpp::packet(beltpp::isocket_drop()));
                    remove_peer();
                }
                throw;
            }
            catch (too_long_string_exception const& e)
            {
                if (it == interface_type::rpc)
                {
                    TooLongString msg;
                    beltpp::assign(msg, e);
                    psk->send(peerid, beltpp::packet(msg));
                }
                else
                {
                    psk->send(peerid, beltpp::packet(beltpp::isocket_drop()));
                    remove_peer();
                }
                throw;
            }
            catch (uri_exception const& e)
            {
                if (it == interface_type::rpc)
                {
                    UriError msg;
                    beltpp::assign(msg, e);
                    psk->send(peerid, beltpp::packet(msg));
                }
                else
                {
                    psk->send(peerid, beltpp::packet(beltpp::isocket_drop()));
                    remove_peer();
                }
                throw;
            }
            catch (std::exception const& e)
            {
                if (it == interface_type::rpc)
                {
                    RemoteError msg;
                    msg.message = e.what();
                    psk->send(peerid, beltpp::packet(msg));
                }
                throw;
            }
            catch (...)
            {
                if (it == interface_type::rpc)
                {
                    RemoteError msg;
                    msg.message = "unknown exception";
                    psk->send(peerid, beltpp::packet(msg));
                }
                throw;
            }
            }   // for (auto& received_packet : received_packets)
        }   // for (auto& pevent_item : wait_sockets)
    }

    if (wait_result & beltpp::event_handler::timer_out)
    {
        m_pimpl->m_ptr_p2p_socket->timer_action();
        m_pimpl->m_ptr_rpc_socket->timer_action();
    }

    if (m_pimpl->m_slave_node &&
        wait_result & beltpp::event_handler::on_demand)
    {
        beltpp::socket::packets received_packets = m_pimpl->m_slave_node->receive();
        for (auto& ref_packet : received_packets)
        {
            m_pimpl->m_sessions.process("slave", std::move(ref_packet));
        }
    }

    m_pimpl->m_nodeid_sessions.erase_all_pending();
    m_pimpl->m_sessions.erase_all_pending();

    // channels and storages connect to slave threads
    if (m_pimpl->m_reconnect_timer.expired())
    {
        m_pimpl->m_reconnect_timer.update();

        m_pimpl->reconnect_slave();
    }

//    // test ! print summary report about connections
//    if (m_pimpl->m_summary_report_timer.expired())
//    {
//        m_pimpl->m_summary_report_timer.update();
//
//        m_pimpl->writeln_node("Summary Report");
//        m_pimpl->writeln_node("    p2p nodes connected");
//        if (m_pimpl->m_p2p_peers.empty())
//            m_pimpl->writeln_node("        none");
//        else
//        {
//            for (auto const& item : m_pimpl->m_p2p_peers)
//                m_pimpl->writeln_node("        " + detail::peer_short_names(item));
//        }
//        m_pimpl->writeln_node("    blockchain heigth: " +
//                              std::to_string(m_pimpl->m_blockchain.length()));
//        m_pimpl->writeln_node("End Summary Report");
//    }

    // broadcast own transactions to all peers for the case
    // when node could not do this when received it through rpc
    if (m_pimpl->m_broadcast_timer.expired() && !m_pimpl->m_p2p_peers.empty())
    {
        m_pimpl->m_broadcast_timer.update();

        size_t pool_size = m_pimpl->m_transaction_pool.length();
        if (pool_size > 0)
        {
            m_pimpl->writeln_node("broadcasting old stored transactions to all peers");

            auto current_time = system_clock::now();

            for (size_t pool_index = 0; pool_index != pool_size; ++pool_index)
            {
                SignedTransaction const& signed_transaction = m_pimpl->m_transaction_pool.at(pool_index);

                if (current_time < system_clock::from_time_t(signed_transaction.transaction_details.expiry.tm) &&
                    current_time > system_clock::from_time_t(signed_transaction.transaction_details.creation.tm) + chrono::seconds(BLOCK_MINE_DELAY))
                {
                    Broadcast broadcast;
                    broadcast.echoes = 2;
                    broadcast.package = signed_transaction;

                    broadcast_message(std::move(broadcast),
                                      m_pimpl->m_ptr_p2p_socket->name(),
                                      m_pimpl->m_ptr_p2p_socket->name(),
                                      true, // like from rpc
                                      nullptr, // no logger
                                      m_pimpl->m_p2p_peers,
                                      m_pimpl->m_ptr_p2p_socket.get());
                }
            }
        }
    }

    // clean old transaction keys from cache
    // to minimize it and make it work faster
    if (m_pimpl->m_cache_cleanup_timer.expired())
    {
        m_pimpl->m_cache_cleanup_timer.update();

        m_pimpl->clean_transaction_cache();

        m_pimpl->m_slave_tasks.clean();

        //  temp place
        broadcast_node_type(m_pimpl);
        broadcast_address_info(m_pimpl);

        //  yes temp place still
        //  collect up to 100 addresses to check
        size_t collected = 0;
        for (auto& nodeid_item : m_pimpl->m_nodeid_service.nodeids)
        {
            if (collected == 100)
                break;

            for (auto& address : nodeid_item.second.get())
            {
                auto ptr_action = nodeid_item.second.take_action(address);

                if (ptr_action)
                {
                    m_pimpl->writeln_node(std::to_string(nodeid_item.second.is_verified(address)) +
                        " - " + nodeid_item.first + ": " +
                        address.to_string());

                    vector<unique_ptr<meshpp::session_action<meshpp::nodeid_session_header>>> actions;
                    actions.emplace_back(new session_action_connections(*m_pimpl->m_ptr_rpc_socket.get()));
                    actions.emplace_back(new session_action_signatures(*m_pimpl->m_ptr_rpc_socket.get(),
                                                                        m_pimpl->m_nodeid_service));

                    actions.emplace_back(std::move(ptr_action));

                    meshpp::nodeid_session_header header;
                    header.nodeid = nodeid_item.first;
                    header.address = address;
                    m_pimpl->m_nodeid_sessions.add(header,
                                                   std::move(actions),
                                                   chrono::minutes(1));
                }
                ++collected;
            }
        }
    }

    // init sync process and block mining
    if (m_pimpl->m_check_timer.expired())
    {
        m_pimpl->m_check_timer.update();

        for (auto const& peerid : m_pimpl->m_p2p_peers)
        {
            vector<unique_ptr<meshpp::session_action<meshpp::nodeid_session_header>>> actions;
            actions.emplace_back(new session_action_p2pconnections(*m_pimpl->m_ptr_p2p_socket.get(),
                                                                   *m_pimpl.get()));
            actions.emplace_back(new session_action_sync_request(*m_pimpl.get()));

            meshpp::nodeid_session_header header;
            header.nodeid = peerid;
            header.address = m_pimpl->m_ptr_p2p_socket->info_connection(peerid);
            m_pimpl->m_nodeid_sessions.add(header,
                                           std::move(actions),
                                           chrono::seconds(SYNC_TIMER));
        }

        //  work through process of block header sync or mining
        {
            // process collected SyncResponse data
            BlockHeader const& head_block_header = m_pimpl->m_blockchain.last_header();

            uint64_t scan_block_number = head_block_header.block_number;
            uint64_t scan_consensus_sum = head_block_header.c_sum;
            beltpp::isocket::peer_id scan_peer;

            //  the duration passed according to my system time since the head block
            //  was signed
            chrono::system_clock::duration since_head_block =
                    system_clock::now() -
                    system_clock::from_time_t(head_block_header.time_signed.tm);

            for (auto& it : m_pimpl->all_sync_info.sync_responses)
            {
                if (m_pimpl->m_p2p_peers.find(it.first) == m_pimpl->m_p2p_peers.end())
                {
                    assert(false); // because the session must handle this
                    continue; // for the case if peer is droped before sync started
                }

                if (scan_consensus_sum < it.second.own_header.c_sum &&
                    scan_block_number <= it.second.own_header.block_number)
                {
                    scan_block_number = it.second.own_header.block_number;
                    scan_consensus_sum = it.second.own_header.c_sum;
                    scan_peer = it.first;
                }
            }

            bool mine_now = false;
            bool sync_now = false;
            bool far_behind = (head_block_header.block_number + 1 < scan_block_number);
            bool just_same = (head_block_header.block_number == scan_block_number);
            B_UNUSED(just_same);

            auto net_sum = m_pimpl->all_sync_info.net_sync_info().c_sum;
            auto own_sum = m_pimpl->all_sync_info.own_sync_info().c_sum;

            if (far_behind &&
                false == m_pimpl->all_sync_info.blockchain_sync_in_progress)
            {
                sync_now = true;
                assert(false == just_same);
            }
            else if (false == m_pimpl->all_sync_info.blockchain_sync_in_progress)
            {
                //  just one block behind or just same
                //
                if (own_sum < scan_consensus_sum &&
                    net_sum < scan_consensus_sum)
                {
                    //  direct peer has ready excellent chain to offer
                    sync_now = true;
                    assert(false == just_same);
                }
                else if (own_sum < scan_consensus_sum &&
                         net_sum == scan_consensus_sum)
                {
                    //  direct peer finally got the excellent chain that was promised before
                    sync_now = true;
                    assert(false == just_same);
                }
                else if (own_sum < scan_consensus_sum/* &&
                         net_sum > scan_consensus_sum*/)
                {
                    //  the promised excellent block did not arrive yet
                    //  and the network has ready better block than I can mine
                    if (chrono::seconds(BLOCK_MINE_DELAY + BLOCK_WAIT_DELAY) < since_head_block)
                    {
                        //  and it is too late already, can't wait anymore
                        sync_now = true;
                        assert(false == just_same);
                    }
                    //  otherwise will just wait
                }
                else// if (own_sum >= scan_consensus_sum)
                {
                    //  I can mine better block and don't need received data
                    //  I can wait until it is
                    //  either past BLOCK_MINE_DELAY and I can mine better than scan_consensus_sum
                    //  or it is past BLOCK_MINE_DELAY + BLOCK_WAIT_DELAY and I can mine better than net_sum
                    if (chrono::seconds(BLOCK_MINE_DELAY + BLOCK_WAIT_DELAY) < since_head_block ||
                        (
                            chrono::seconds(BLOCK_MINE_DELAY) <= since_head_block &&
                            own_sum >= net_sum
                        ))
                    {
                        mine_now = true;
                        assert(false == sync_now);
                    }
                }
            }

            assert(false == sync_now || false == mine_now);

            if (sync_now)
            {
                assert(false == just_same);
                assert(false == scan_peer.empty());
                vector<unique_ptr<meshpp::session_action<meshpp::nodeid_session_header>>> actions;
                actions.emplace_back(new session_action_header(*m_pimpl.get(),
                                                               scan_block_number,
                                                               scan_consensus_sum));
                actions.emplace_back(new session_action_block(*m_pimpl.get()));

                meshpp::nodeid_session_header header;
                header.nodeid = scan_peer;
                header.address = m_pimpl->m_ptr_p2p_socket->info_connection(scan_peer);
                m_pimpl->m_nodeid_sessions.add(header,
                                               std::move(actions),
                                               chrono::seconds(SYNC_TIMER));
            }
            else if (m_pimpl->is_miner() &&
                     mine_now)
            {
                mine_block(m_pimpl);
            }
        }
    }

    return code;
}

void node::set_slave_node(storage_node& slave_node)
{
    m_pimpl->m_slave_node = &slave_node;
}

}


