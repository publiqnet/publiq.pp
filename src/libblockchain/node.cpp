#include "node.hpp"
#include "common.hpp"
#include "exception.hpp"

#include "communication_rpc.hpp"
#include "communication_p2p.hpp"
#include "transaction_handler.hpp"

#include "open_container_packet.hpp"
#include "sessions.hpp"
#include "message.tmpl.hpp"
#include "storage_node_internals.hpp"
#include "types.hpp"

#include <publiq.pp/storage_utility_rpc.hpp>

#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <exception>

using namespace BlockchainMessage;

using beltpp::packet;
using beltpp::socket;
using beltpp::ip_address;
using peer_id = socket::peer_id;

namespace chrono = std::chrono;
using chrono::system_clock;

using std::pair;
using std::string;
using std::vector;
using std::unique_ptr;
using std::unordered_set;

namespace publiqpp
{
//  free functions
void sync_worker(detail::node_internals& impl);
/*
 * node
 */
node::node(string const& genesis_signed_block,
           ip_address const& public_address,
           beltpp::ip_address const& public_ssl_address,
           ip_address const& rpc_bind_to_address,
           ip_address const& p2p_bind_to_address,
           std::vector<ip_address> const& p2p_connect_to_addresses,
           filesystem::path const& fs_blockchain,
           filesystem::path const& fs_action_log,
           filesystem::path const& fs_transaction_pool,
           filesystem::path const& fs_state,
           filesystem::path const& fs_documents,
           filesystem::path const& fs_storages,
           filesystem::path const& fs_storage,
           filesystem::path const& fs_black_box,
           beltpp::ilog* plogger_p2p,
           beltpp::ilog* plogger_node,
           meshpp::private_key const& pv_key,
           NodeType const& n_type,
           uint64_t fractions,
           uint64_t freeze_before_block,
           uint64_t revert_blocks_count,
           string const& manager_address,
           bool log_enabled,
           bool transfer_only,
           bool testnet,
           bool resync,
           bool discovery_server,
           coin const& mine_amount_threshhold,
           std::vector<coin> const& block_reward_array,
           detail::fp_counts_per_channel_views p_counts_per_channel_views)
    : m_pimpl(new detail::node_internals(genesis_signed_block,
                                         public_address,
                                         public_ssl_address,
                                         rpc_bind_to_address,
                                         p2p_bind_to_address,
                                         p2p_connect_to_addresses,
                                         fs_blockchain,
                                         fs_action_log,
                                         fs_transaction_pool,
                                         fs_state,
                                         fs_documents,
                                         fs_storages,
                                         fs_storage,
                                         fs_black_box,
                                         plogger_p2p,
                                         plogger_node,
                                         pv_key,
                                         n_type,
                                         fractions,
                                         freeze_before_block,
                                         revert_blocks_count,
                                         manager_address,
                                         log_enabled,
                                         transfer_only,
                                         testnet,
                                         resync,
                                         discovery_server,
                                         mine_amount_threshhold,
                                         block_reward_array,
                                         p_counts_per_channel_views))
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

void node::run(bool& stop_check)
{
    stop_check = false;

    if (m_pimpl->m_initialize)
    {
        beltpp::on_failure guard_initialize([&stop_check]{ stop_check = true; });
        stop_check = m_pimpl->initialize();
        guard_initialize.dismiss();
        return;
    }

    if (m_pimpl->m_service_statistics_broadcast_triggered)
    {
        m_pimpl->m_service_statistics_broadcast_triggered = false;
        broadcast_service_statistics(*m_pimpl);
    }

    auto wait_result = m_pimpl->wait_and_receive_one();

    if (wait_result.et == detail::wait_result_item::event)
    {
        auto peerid = wait_result.peerid;
        auto received_packet = std::move(wait_result.packet);
        auto it = wait_result.it;

        beltpp::isocket* psk = nullptr;
        if (it == detail::wait_result_item::interface_type::p2p)
            psk = m_pimpl->m_ptr_p2p_socket.get();
        else if (it == detail::wait_result_item::interface_type::rpc)
            psk = m_pimpl->m_ptr_rpc_socket.get();

        if (nullptr == psk)
            throw std::logic_error("nullptr == psk");

        try
        {
            if (false == m_pimpl->m_nodeid_sessions.process(peerid, std::move(received_packet)) &&
                false == m_pimpl->m_sync_sessions.process(peerid, std::move(received_packet)))
            {
                vector<packet*> composition;

                open_container_packet<Broadcast, SignedTransaction> broadcast_signed_transaction;
                open_container_packet<Broadcast> broadcast_anything;
                bool is_container = broadcast_signed_transaction.open(received_packet, composition, *m_pimpl.get()) ||
                                    broadcast_anything.open(received_packet, composition, *m_pimpl.get());

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
                    if (it == detail::wait_result_item::interface_type::p2p)
                        m_pimpl->writeln_node("joined: " + detail::peer_short_names(peerid) + 
                                              " -> total:" + std::to_string(m_pimpl->m_p2p_peers.size() + 1));

                    if (it == detail::wait_result_item::interface_type::p2p)
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

                    break;
                }
                case beltpp::isocket_drop::rtt:
                {
                    if (it == detail::wait_result_item::interface_type::p2p)
                    {
                        m_pimpl->remove_peer(peerid);
                        m_pimpl->writeln_node("dropped: " + detail::peer_short_names(peerid) +
                                              " -> total:" + std::to_string(m_pimpl->m_p2p_peers.size()));
                    }

                    break;
                }
                case beltpp::isocket_protocol_error::rtt:
                {
                    beltpp::isocket_protocol_error msg;
                    ref_packet.get(msg);
                    m_pimpl->writeln_node("protocol error: " + detail::peer_short_names(peerid));
                    m_pimpl->writeln_node(msg.buffer);
                    psk->send(peerid, beltpp::packet(beltpp::isocket_drop()));

                    if (it == detail::wait_result_item::interface_type::p2p)
                        m_pimpl->remove_peer(peerid);
                    
                    break;
                }
                case beltpp::isocket_open_refused::rtt:
                {
                    beltpp::isocket_open_refused msg;
                    ref_packet.get(msg);
                    //m_pimpl->writeln_node_warning(msg.reason + ", " + peerid);
                    break;
                }
                case beltpp::isocket_open_error::rtt:
                {
                    beltpp::isocket_open_error msg;
                    ref_packet.get(msg);
                    //m_pimpl->writeln_node_warning(msg.reason + ", " + peerid);
                    break;
                }
                case Transfer::rtt:
                case File::rtt:
                case ContentUnit::rtt:
                case Content::rtt:
                case Role::rtt:
                case StorageUpdate::rtt:
                case ServiceStatistics::rtt:
                case SponsorContentUnit::rtt:
                case CancelSponsorContentUnit::rtt:
                {
                    if (m_pimpl->m_blockchain.length() >= m_pimpl->m_freeze_before_block)
                        break;

                    if (broadcast_signed_transaction.items.empty())
                        throw wrong_data_exception("will process only \"broadcast signed transaction\"");

                    if (m_pimpl->m_transfer_only && ref_packet.type() != Transfer::rtt)
                        throw std::runtime_error("this is coin only blockchain");

                    Broadcast* p_broadcast = nullptr;
                    SignedTransaction* p_signed_tx = nullptr;

                    broadcast_signed_transaction.items[0]->get(p_broadcast);
                    broadcast_signed_transaction.items[1]->get(p_signed_tx);

                    assert(p_broadcast);
                    assert(p_signed_tx);

                    Broadcast& broadcast = *p_broadcast;
                    SignedTransaction& signed_tx = *p_signed_tx;

                    if (action_process_on_chain(signed_tx, *m_pimpl.get()))
                    {
                        broadcast_message(std::move(broadcast),
                                          m_pimpl->m_ptr_p2p_socket->name(),
                                          peerid,
                                          it == detail::wait_result_item::interface_type::rpc,
                                          nullptr,
                                          m_pimpl->m_p2p_peers,
                                          m_pimpl->m_ptr_p2p_socket.get());
                    }
                
                    if (it == detail::wait_result_item::interface_type::rpc)
                        psk->send(peerid, beltpp::packet(Done()));

                    break;
                }
                case AddressInfo::rtt:
                {
                    if (broadcast_signed_transaction.items.empty())
                        throw wrong_data_exception("will process only \"broadcast signed transaction\"");

                    if (it != detail::wait_result_item::interface_type::p2p)
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
                        beltpp::ip_address beltpp_ssl_ip_address;
                        beltpp::assign(beltpp_ssl_ip_address, address_info.ssl_ip_address);

                        m_pimpl->m_nodeid_service.add(address_info.node_address,
                                                      beltpp_ip_address,
                                                      beltpp_ssl_ip_address,
                                                      unique_ptr<session_action_broadcast_address_info>(
                                                          new session_action_broadcast_address_info(*m_pimpl.get(),
                                                                                                    peerid,
                                                                                                    std::move(broadcast))));
                    }

                    break;
                }
                case StorageUpdateCommand::rtt:
                {
                    if (broadcast_signed_transaction.items.empty())
                        throw wrong_data_exception("will process only \"broadcast signed transaction\"");

                    Broadcast* p_broadcast = nullptr;
                    SignedTransaction* p_signed_tx = nullptr;
                    StorageUpdateCommand* p_update_command = nullptr;

                    broadcast_signed_transaction.items[0]->get(p_broadcast);
                    broadcast_signed_transaction.items[1]->get(p_signed_tx);
                    ref_packet.get(p_update_command);

                    assert(p_broadcast);
                    assert(p_signed_tx);
                    assert(p_update_command);

                    Broadcast& broadcast = *p_broadcast;
                    SignedTransaction& signed_tx = *p_signed_tx;
                    StorageUpdateCommand& update_command = *p_update_command;

                    if (process_update_command(signed_tx, update_command, m_pimpl))
                    {
                        if (update_command.storage_address == m_pimpl->m_pb_key.to_string())
                        {
                            // command is addressed to me
                            if (signed_tx.authorizations[0].address == m_pimpl->m_manager_address)
                            {
                                if (update_command.status == UpdateType::remove)
                                {
                                    delete_storage_file(*m_pimpl.get(), psk, peerid, update_command.file_uri);
                                }
                                else // update_command.status == UpdateType::save
                                {
                                    m_pimpl->m_storage_controller.enqueue(update_command.file_uri, update_command.channel_address);
                                }
                            }
                        }
                        else
                        {
                            // rebroadcast command direct peer or to all
                            std::unordered_set<beltpp::isocket::peer_id> broadcast_peers;

                            if (0 == m_pimpl->m_p2p_peers.count(update_command.storage_address))
                                broadcast_peers = m_pimpl->m_p2p_peers;
                            else
                                broadcast_peers.insert(update_command.storage_address);

                            broadcast_message(std::move(broadcast),
                                              m_pimpl->m_ptr_p2p_socket->name(),
                                              peerid,
                                              true,
                                              nullptr,
                                              broadcast_peers,
                                              m_pimpl->m_ptr_p2p_socket.get());
                        }
                    }

                    if (it == detail::wait_result_item::interface_type::rpc)
                        psk->send(peerid, beltpp::packet(Done()));

                    break;
                }
                case StorageFile::rtt:
                {
                    if (NodeType::blockchain == m_pimpl->m_node_type ||
                        nullptr == m_pimpl->m_slave_node ||
                        it != detail::wait_result_item::interface_type::rpc ||
                        m_pimpl->m_ptr_rpc_socket->get_peer_type(peerid) != beltpp::socket::peer_type::streaming_accepted)
                        throw wrong_request_exception("Do not disturb!");

                    StorageFile storage_file;
                    std::move(ref_packet).get(storage_file);

                    auto* pimpl = m_pimpl.get();
                    std::function<void(beltpp::packet&&)> callback_lambda =
                            [psk, peerid, pimpl](beltpp::packet&& package)
                    {
                        if (NodeType::storage == pimpl->m_node_type &&
                            package.type() == StorageFileAddress::rtt)
                        {
                            StorageFileAddress* pfile_address;
                            package.get(pfile_address);
                            if (false == pimpl->m_documents.storage_has_uri(pfile_address->uri,
                                                                            pimpl->m_pb_key.to_string()))
                                broadcast_storage_update(*pimpl, pfile_address->uri, UpdateType::store);
                        }

                        if (false == package.empty())
                            psk->send(peerid, std::move(package));
                    };

                    vector<unique_ptr<meshpp::session_action<meshpp::session_header>>> actions;
                    actions.emplace_back(new session_action_save_file(*pimpl,
                                                                      std::move(storage_file),
                                                                      callback_lambda));

                    meshpp::session_header header;
                    header.peerid = "slave";
                    m_pimpl->m_sessions.add(header,
                                            std::move(actions),
                                            chrono::minutes(1));
                    
                    break;
                }
                case StorageFileDelete::rtt:
                {
                    if (NodeType::blockchain == m_pimpl->m_node_type ||
                        nullptr == m_pimpl->m_slave_node ||
                        it != detail::wait_result_item::interface_type::rpc ||
                        m_pimpl->m_ptr_rpc_socket->get_peer_type(peerid) != beltpp::socket::peer_type::streaming_accepted)
                        throw wrong_request_exception("Do not disturb!");

                    StorageFileDelete storage_file_delete;
                    std::move(ref_packet).get(storage_file_delete);

                    delete_storage_file(*m_pimpl.get(), psk, peerid, storage_file_delete.uri);

                    //auto* pimpl = m_pimpl.get();
                    //std::function<void(beltpp::packet&&)> callback_lambda =
                    //        [psk, peerid, storage_file_delete, pimpl](beltpp::packet&& package)
                    //{
                    //    if (NodeType::storage == pimpl->m_node_type &&
                    //        package.type() == Done::rtt)
                    //    {
                    //        broadcast_storage_update(*pimpl, storage_file_delete.uri, UpdateType::remove);
                    //    }
                    //
                    //    if (false == package.empty())
                    //        psk->send(peerid, std::move(package));
                    //};
                    //
                    //vector<unique_ptr<meshpp::session_action<meshpp::session_header>>> actions;
                    //actions.emplace_back(new session_action_delete_file(*m_pimpl.get(),
                    //                                                    storage_file_delete.uri,
                    //                                                    callback_lambda));
                    //
                    //meshpp::session_header header;
                    //header.peerid = "slave";
                    //m_pimpl->m_sessions.add(header,
                    //                        std::move(actions),
                    //                        chrono::minutes(1));

                    break;
                }
                case FileUrisRequest::rtt:
                {
                    if (NodeType::blockchain == m_pimpl->m_node_type ||
                        nullptr == m_pimpl->m_slave_node ||
                        it != detail::wait_result_item::interface_type::rpc ||
                        m_pimpl->m_ptr_rpc_socket->get_peer_type(peerid) != beltpp::socket::peer_type::streaming_accepted)
                        throw wrong_request_exception("Do not disturb!");

                    std::function<void(beltpp::packet&&)> callback_lambda =
                            [psk, peerid](beltpp::packet&& package)
                    {
                        psk->send(peerid, std::move(package));
                    };

                    vector<unique_ptr<meshpp::session_action<meshpp::session_header>>> actions;
                    actions.emplace_back(new session_action_get_file_uris(*m_pimpl.get(),
                                                                          callback_lambda));

                    meshpp::session_header header;
                    header.peerid = "slave";
                    m_pimpl->m_sessions.add(header,
                                            std::move(actions),
                                            chrono::minutes(1));

                    break;
                }
                case LoggedTransactionsRequest::rtt:
                {
                    if (it == detail::wait_result_item::interface_type::rpc)
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
                case PublicAddressesRequest::rtt:
                {
                    PublicAddressesRequest msg;
                    std::move(ref_packet).get(msg);

                    if (msg.address_type == PublicAddressType::rpc)
                        get_public_addresses(*psk, peerid, *m_pimpl.get());
                    else
                        get_peers_addresses(*psk, peerid, *m_pimpl.get());

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
                    BlockHeaderExtended const& header_ex = m_pimpl->m_blockchain.last_header_ex();

                    SyncResponse sync_response;
                    sync_response.own_header = header_ex;

                    if (m_pimpl->all_sync_info.net_sync_info().c_sum > m_pimpl->all_sync_info.own_sync_info().c_sum)
                        sync_response.promised_header = m_pimpl->all_sync_info.net_sync_info();
                    else
                        sync_response.promised_header = m_pimpl->all_sync_info.own_sync_info();

                    psk->send(peerid, beltpp::packet(std::move(sync_response)));

                    break;
                }
                case BlockHeaderRequest::rtt:
                {
                    BlockHeaderRequest header_request;
                    std::move(ref_packet).get(header_request);

                    session_action_header::process_request(peerid,
                                                           header_request,
                                                           *m_pimpl.get());

                    break;
                }
                case BlockchainRequest::rtt:
                {
                    if (it != detail::wait_result_item::interface_type::p2p)
                        throw wrong_request_exception("BlockchainRequest received through rpc!");

                    BlockchainRequest blockchain_request;
                    std::move(ref_packet).get(blockchain_request);

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
                    authorization.signature = pv.sign(transaction_broadcast_request.transaction_details.to_string()).base58;

                    BlockchainMessage::SignedTransaction signed_transaction;
                    signed_transaction.transaction_details = transaction_broadcast_request.transaction_details;
                    signed_transaction.authorizations.push_back(authorization);

                    TransactionDone transaction_done;
                    transaction_done.transaction_hash = meshpp::hash(signed_transaction.to_string());

                    signed_transaction_validate(signed_transaction,
                                                std::chrono::system_clock::now(),
                                                std::chrono::seconds(NODES_TIME_SHIFT),
                                                *m_pimpl.get());

                    if (action_process_on_chain(signed_transaction, *m_pimpl.get()))
                    {
                        BlockchainMessage::Broadcast broadcast;
                        broadcast.echoes = 2;
                        broadcast.package = std::move(signed_transaction);

                        broadcast_message(std::move(broadcast),
                                          m_pimpl->m_ptr_p2p_socket->name(),
                                          peerid,
                                          it == detail::wait_result_item::interface_type::rpc,
                                          //m_pimpl->plogger_node,
                                          nullptr,
                                          m_pimpl->m_p2p_peers,
                                          m_pimpl->m_ptr_p2p_socket.get());
                    }

                    psk->send(peerid, beltpp::packet(std::move(transaction_done)));

                    break;
                }
                case BlackBox::rtt:
                {
                    if (it != detail::wait_result_item::interface_type::p2p)
                        throw wrong_request_exception("BlackBox received through rpc!");

                    if (broadcast_signed_transaction.items.empty())
                        throw wrong_data_exception("will process only \"broadcast signed transaction\"");

                    Broadcast* p_broadcast = nullptr;
                    SignedTransaction* p_signed_tx = nullptr;
                    BlackBox* p_black_box = nullptr;

                    broadcast_signed_transaction.items[0]->get(p_broadcast);
                    broadcast_signed_transaction.items[1]->get(p_signed_tx);
                    ref_packet.get(p_black_box);

                    assert(p_broadcast);
                    assert(p_signed_tx);
                    assert(p_black_box);

                    Broadcast& broadcast = *p_broadcast;
                    SignedTransaction& signed_transaction = *p_signed_tx;
                    BlackBox& black_box = *p_black_box;

                    if (process_black_box(signed_transaction, black_box, m_pimpl))
                    {
                        if (black_box.to == m_pimpl->m_pb_key.to_string())
                        {
                            // message is addressed to me
                            save_black_box(black_box, m_pimpl);
                        }
                        else
                        {
                            // rebroadcast message direct peer or to all
                            std::unordered_set<beltpp::isocket::peer_id> broadcast_peers;

                            if (0 == m_pimpl->m_p2p_peers.count(black_box.to))
                                broadcast_peers = m_pimpl->m_p2p_peers;
                            else
                                broadcast_peers.insert(black_box.to);

                            broadcast_message(std::move(broadcast),
                                              m_pimpl->m_ptr_p2p_socket->name(),
                                              peerid,
                                              true,
                                              nullptr,
                                              broadcast_peers,
                                              m_pimpl->m_ptr_p2p_socket.get());
                        }
                    }

                    if (it == detail::wait_result_item::interface_type::rpc)
                        psk->send(peerid, beltpp::packet(Done()));

                    break;
                }
                case BlackBoxBroadcastRequest::rtt:
                {
                    if (it != detail::wait_result_item::interface_type::rpc)
                        throw wrong_request_exception("BlackBoxBroadcastRequest received through p2p!");

                    BlackBoxBroadcastRequest black_box_boadcast_request;
                    std::move(ref_packet).get(black_box_boadcast_request);

                    Transaction transaction;
                    transaction.action = std::move(black_box_boadcast_request.broadcast_black_box);
                    transaction.creation.tm = system_clock::to_time_t(system_clock::now());
                    transaction.expiry.tm = system_clock::to_time_t(system_clock::now() + chrono::hours(TRANSACTION_MAX_LIFETIME_HOURS));
                    m_pimpl->m_fee_transactions.to_Coin(transaction.fee);

                    Authority authorization;
                    meshpp::private_key pv(m_pimpl->m_pb_key.to_string());
                    authorization.address = pv.get_public_key().to_string();
                    authorization.signature = pv.sign(transaction.to_string()).base58;

                    BlockchainMessage::SignedTransaction signed_transaction;
                    signed_transaction.transaction_details = transaction;
                    signed_transaction.authorizations.push_back(authorization);

                    TransactionDone transaction_done;
                    transaction_done.transaction_hash = meshpp::hash(signed_transaction.to_string());

                    if (process_black_box(signed_transaction, black_box_boadcast_request.broadcast_black_box, m_pimpl))
                    {
                        BlockchainMessage::Broadcast broadcast;
                        broadcast.echoes = 2;
                        broadcast.package = std::move(signed_transaction);

                        broadcast_message(std::move(broadcast),
                                          m_pimpl->m_ptr_p2p_socket->name(),
                                          peerid,
                                          it == detail::wait_result_item::interface_type::rpc,
                                          //m_pimpl->plogger_node,
                                          nullptr,
                                          m_pimpl->m_p2p_peers,
                                          m_pimpl->m_ptr_p2p_socket.get());
                    }

                    psk->send(peerid, beltpp::packet(std::move(transaction_done)));

                    break;
                }
                case BlackBoxRequest::rtt:
                {
                    BlackBoxResponse response;

                    for (size_t it = 0; it != m_pimpl->m_black_box.length(); ++it)
                        response.holded_boxes.push_back(m_pimpl->m_black_box.at(it));

                    beltpp::on_failure guard([this]
                    {
                        m_pimpl->m_black_box.discard();
                    });

                    m_pimpl->m_black_box.clear();

                    guard.dismiss();

                    m_pimpl->m_black_box.commit();

                    psk->send(peerid, beltpp::packet(std::move(response)));

                    break;
                }
                case Ping::rtt:
                {
                    Pong msg_pong;
                    msg_pong.node_address = m_pimpl->m_pv_key.get_public_key().to_string();
                    msg_pong.stamp.tm = system_clock::to_time_t(system_clock::now());
                    string message_pong = msg_pong.node_address + ::beltpp::gm_time_t_to_gm_string(msg_pong.stamp.tm);
                    auto signed_message = m_pimpl->m_pv_key.sign(message_pong);

                    msg_pong.signature = std::move(signed_message.base58);
                    psk->send(peerid, beltpp::packet(std::move(msg_pong)));
                    break;
                }
                case Served::rtt:
                {
                    if (NodeType::channel != m_pimpl->m_node_type ||
                        it != detail::wait_result_item::interface_type::rpc ||
                        m_pimpl->m_ptr_rpc_socket->get_peer_type(peerid) != beltpp::socket::peer_type::streaming_accepted)
                        throw wrong_request_exception("Do not disturb!");

                    Served msg;
                    std::move(ref_packet).get(msg);

                    detail::service_counter::service_unit unit;
                    detail::service_counter::service_unit_counter unit_counter;

                    string channel_address;
                    string& storage_address = unit.peer_address;

                    if (false == storage_utility::rpc::verify_storage_order(msg.storage_order_token,
                                                                            channel_address,
                                                                            storage_address,
                                                                            unit.file_uri,
                                                                            unit.content_unit_uri,
                                                                            unit_counter.session_id,
                                                                            unit_counter.seconds,
                                                                            unit_counter.time_point))
                        throw wrong_request_exception("wrong storage order token");

                    if (channel_address != m_pimpl->m_pb_key.to_string())
                        throw wrong_request_exception("channel_address != m_pimpl->m_pb_key.to_string()");

                    m_pimpl->service_counter.served(unit, unit_counter);
#ifdef EXTRA_LOGGING
                    m_pimpl->writeln_node("channel served");
                    m_pimpl->writeln_node(msg.to_string());
#endif

                    psk->send(peerid, beltpp::packet(Done()));

                    break;
                }
                default:
                {
                    //if (ref_packet.type() != SyncResponse::rtt)
                        m_pimpl->writeln_node("master can't handle: " + std::to_string(ref_packet.type()) +
                                              ". peer: " + peerid);

                    break;
                }
                }   // switch ref_packet.type()
            }   // if not processed by sessions
        }
        catch (meshpp::exception_public_key const& e)
        {
            if (it == detail::wait_result_item::interface_type::rpc)
            {
                InvalidPublicKey msg;
                msg.public_key = e.pub_key;
                psk->send(peerid, beltpp::packet(msg));
            }
            else
            {
                psk->send(peerid, beltpp::packet(beltpp::isocket_drop()));
                m_pimpl->remove_peer(peerid);
            }
            throw;
        }
        catch (meshpp::exception_private_key const& e)
        {
            if (it == detail::wait_result_item::interface_type::rpc)
            {
                InvalidPrivateKey msg;
                msg.private_key = e.priv_key;
                psk->send(peerid, beltpp::packet(msg));
            }
            else
            {
                psk->send(peerid, beltpp::packet(beltpp::isocket_drop()));
                m_pimpl->remove_peer(peerid);
            }
            throw;
        }
        catch (meshpp::exception_signature const& e)
        {
            if (it == detail::wait_result_item::interface_type::rpc)
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
                m_pimpl->remove_peer(peerid);
            }
            throw;
        }
        catch (wrong_data_exception const& e)
        {
            if (it == detail::wait_result_item::interface_type::rpc)
            {
                RemoteError remote_error;
                remote_error.message = e.message;
                psk->send(peerid, beltpp::packet(remote_error));
            }
            else
            {
                psk->send(peerid, beltpp::packet(beltpp::isocket_drop()));
                m_pimpl->remove_peer(peerid);
            }
            throw;
        }
        catch (wrong_request_exception const& e)
        {
            if (it == detail::wait_result_item::interface_type::rpc)
            {
                RemoteError remote_error;
                remote_error.message = e.message;
                psk->send(peerid, beltpp::packet(remote_error));
            }
            else
            {
                psk->send(peerid, beltpp::packet(beltpp::isocket_drop()));
                m_pimpl->remove_peer(peerid);
            }
            throw;
        }
        catch (wrong_document_exception const& e)
        {
            if (it == detail::wait_result_item::interface_type::rpc)
            {
                RemoteError remote_error;
                remote_error.message = e.message;
                psk->send(peerid, beltpp::packet(remote_error));
            }
            else
            {
                psk->send(peerid, beltpp::packet(beltpp::isocket_drop()));
                m_pimpl->remove_peer(peerid);
            }
            throw;
        }
        catch (authority_exception const& e)
        {
            if (it == detail::wait_result_item::interface_type::rpc)
            {
                InvalidAuthority msg;
                msg.authority_provided = e.authority_provided;
                msg.authority_required = e.authority_required;
                psk->send(peerid, beltpp::packet(msg));
            }
            else
            {
                psk->send(peerid, beltpp::packet(beltpp::isocket_drop()));
                m_pimpl->remove_peer(peerid);
            }
            throw;
        }
        catch (not_enough_balance_exception const& e)
        {
            if (it == detail::wait_result_item::interface_type::rpc)
            {
                NotEnoughBalance msg;
                e.balance.to_Coin(msg.balance);
                e.spending.to_Coin(msg.spending);
                psk->send(peerid, beltpp::packet(msg));
            }
            else
            {
                psk->send(peerid, beltpp::packet(beltpp::isocket_drop()));
                m_pimpl->remove_peer(peerid);
            }
            throw;
        }
        catch (too_long_string_exception const& e)
        {
            if (it == detail::wait_result_item::interface_type::rpc)
            {
                TooLongString msg;
                beltpp::assign(msg, e);
                psk->send(peerid, beltpp::packet(msg));
            }
            else
            {
                psk->send(peerid, beltpp::packet(beltpp::isocket_drop()));
                m_pimpl->remove_peer(peerid);
            }
            throw;
        }
        catch (uri_exception const& e)
        {
            if (it == detail::wait_result_item::interface_type::rpc)
            {
                UriError msg;
                beltpp::assign(msg, e);
                psk->send(peerid, beltpp::packet(msg));
            }
            else
            {
                psk->send(peerid, beltpp::packet(beltpp::isocket_drop()));
                m_pimpl->remove_peer(peerid);
            }
            throw;
        }
        catch (std::exception const& e)
        {
            if (it == detail::wait_result_item::interface_type::rpc)
            {
                RemoteError msg;
                msg.message = e.what();
                psk->send(peerid, beltpp::packet(msg));
            }
            throw;
        }
        catch (...)
        {
            if (it == detail::wait_result_item::interface_type::rpc)
            {
                RemoteError msg;
                msg.message = "unknown exception";
                psk->send(peerid, beltpp::packet(msg));
            }
            throw;
        }
    }
    else if (wait_result.et == detail::wait_result_item::timer)
    {
        m_pimpl->m_ptr_p2p_socket->timer_action();
        m_pimpl->m_ptr_rpc_socket->timer_action();
    }
    else if (m_pimpl->m_slave_node && wait_result.et == detail::wait_result_item::on_demand)
    {
        auto ref_packet = std::move(wait_result.packet);

        if (false == m_pimpl->m_sessions.process("slave", std::move(ref_packet)))
        {
            switch (ref_packet.type())
            {
            case StorageTypes::ContainerMessage::rtt:
            {
                StorageTypes::ContainerMessage msg_container;
                std::move(ref_packet).get(msg_container);

                if (msg_container.package.type() == Served::rtt)
                {
                    Served msg;
                    std::move(msg_container.package).get(msg);
                    if (m_pimpl->m_node_type == NodeType::storage)
                    {
                        detail::service_counter::service_unit unit;
                        detail::service_counter::service_unit_counter unit_counter;

                        string& channel_address = unit.peer_address;
                        string storage_address;

                        if (storage_utility::rpc::verify_storage_order(msg.storage_order_token,
                                                                       channel_address,
                                                                       storage_address,
                                                                       unit.file_uri,
                                                                       unit.content_unit_uri,
                                                                       unit_counter.session_id,
                                                                       unit_counter.seconds,
                                                                       unit_counter.time_point) &&
                            storage_address == m_pimpl->m_pb_key.to_string() &&
                            m_pimpl->m_documents.file_exists(unit.file_uri))
                        {
                            unit.content_unit_uri.clear(); // simulate the old behavior

                            m_pimpl->service_counter.served(unit, unit_counter);
#ifdef EXTRA_LOGGING
                            m_pimpl->writeln_node("storage served");
                            m_pimpl->writeln_node(msg.to_string());
#endif
                        }

                    }
                }
                break;
            }
            }
        }   // if not processed by sessions
    }

    m_pimpl->m_sessions.erase_all_pending();
    m_pimpl->m_sync_sessions.erase_all_pending();
    m_pimpl->m_nodeid_sessions.erase_all_pending();

    // broadcast own transactions to all peers for the case
    // when node could not do this when received it through rpc
    if (m_pimpl->m_broadcast_timer.expired() && !m_pimpl->m_p2p_peers.empty())
    {
        m_pimpl->m_broadcast_timer.update();

        size_t pool_size = m_pimpl->m_transaction_pool.length();
        if (pool_size > 0 && m_pimpl->blockchain_updated())
        {
            //m_pimpl->writeln_node("broadcasting old stored transactions to all peers");

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

        //  temp place
        m_pimpl->m_nodeid_service.take_actions([this](std::string const& node_address,
                                                      beltpp::ip_address const& address,
                                                      std::unique_ptr<session_action_broadcast_address_info>&& ptr_action)
        {
            vector<unique_ptr<meshpp::session_action<meshpp::nodeid_session_header>>> actions;
            actions.emplace_back(new session_action_connections(*m_pimpl->m_ptr_rpc_socket.get()));
            actions.emplace_back(new session_action_signatures(*m_pimpl->m_ptr_rpc_socket.get(),
                                                                m_pimpl->m_nodeid_service));

            actions.emplace_back(std::move(ptr_action));

            meshpp::nodeid_session_header header;
            header.nodeid = node_address;
            header.address = address;
            m_pimpl->m_nodeid_sessions.add(header,
                                           std::move(actions),
                                           chrono::minutes(1));
        });

        // collect verified channel addresses and send to slave node
        if (m_pimpl->m_node_type == NodeType::storage &&
            m_pimpl->m_slave_node)
        {
            StorageTypes::SetVerifiedChannels set_channels;

            PublicAddressesInfo public_addresses = m_pimpl->m_nodeid_service.get_addresses();
            for (auto const& item : public_addresses.addresses_info)
            {
                if (item.seconds_since_checked > 2 * PUBLIC_ADDRESS_FRESH_THRESHHOLD_SECONDS)
                    break;

                NodeType check_role;
                if (m_pimpl->m_state.get_role(item.node_address, check_role) &&
                    check_role == NodeType::channel)
                {
                    set_channels.channel_addresses.push_back(item.node_address);
                }
            }

            m_pimpl->m_slave_node->send(beltpp::packet(std::move(set_channels)));
            m_pimpl->m_slave_node->wake();
        }

        //  yes temp place still
        broadcast_node_type(m_pimpl);
        broadcast_address_info(m_pimpl);
    }

    // init sync process and block mining
    if (m_pimpl->m_check_timer.expired())
    {
        m_pimpl->m_check_timer.update();

        if (m_pimpl->m_blockchain.length() < m_pimpl->m_freeze_before_block)
            sync_worker(*m_pimpl.get());

        if (m_pimpl->m_storage_sync_delay.expired() &&
            m_pimpl->blockchain_updated() &&
            m_pimpl->m_transaction_pool.length() < BLOCK_MAX_TRANSACTIONS / 2 &&
            NodeType::storage == m_pimpl->m_node_type &&
            m_pimpl->m_slave_node)
        {
            auto& impl = *m_pimpl.get();

            unordered_map<string, beltpp::ip_address> map_nodeid_ip_address;
            unordered_set<string> set_resolved_nodeids;

            PublicAddressesInfo public_addresses = impl.m_nodeid_service.get_addresses();
            for (auto const& item : public_addresses.addresses_info)
            {
                if (item.seconds_since_checked > 2 * PUBLIC_ADDRESS_FRESH_THRESHHOLD_SECONDS)
                    break;

                NodeType check_role;
                if (impl.m_state.get_role(item.node_address, check_role) &&
                    check_role == NodeType::channel)
                {
                    set_resolved_nodeids.insert(item.node_address);
                    beltpp::assign(map_nodeid_ip_address[item.node_address], item.ip_address);
                }
            }

            auto file_to_channel =
                    impl.m_storage_controller.get_file_requests(set_resolved_nodeids);

            auto get_file_uris_callback = [&impl, file_to_channel, map_nodeid_ip_address](beltpp::packet&& package)
            {
                if (package.type() == BlockchainMessage::FileUris::rtt)
                {
                    BlockchainMessage::FileUris file_uris;
                    std::move(package).get(file_uris);

                    unordered_set<string> set_file_uris;
                    set_file_uris.reserve(file_uris.file_uris.size());

                    for (auto& file_uri : file_uris.file_uris)
                        set_file_uris.insert(std::move(file_uri));

                    using actions_vector = vector<unique_ptr<meshpp::session_action<meshpp::nodeid_session_header>>>;
                    unordered_map<string, actions_vector> map_actions;
                    unordered_map<string, pair<string, bool>> map_broadcast;

                    for (auto const& item : file_to_channel)
                    {
                        auto const& file_uri = item.first;
                        auto const& channel_address = item.second;

                        if (0 == set_file_uris.count(file_uri))
                        {
                            auto& actions = map_actions[channel_address];
                            if (actions.empty())
                            {
                                actions.emplace_back(new session_action_connections(*impl.m_ptr_rpc_socket.get()));
                                actions.emplace_back(new session_action_signatures(*impl.m_ptr_rpc_socket.get(),
                                                                                   impl.m_nodeid_service));
                            }

                            actions.emplace_back(new session_action_request_file(file_uri,
                                                                                 item.second,
                                                                                 impl));
                        }
                        else
                        {
                            map_broadcast[file_uri] = {channel_address, true};
                        }
                    }

                    beltpp::finally guard_map_broadcast([&impl, &map_broadcast]
                    {
                        for (auto const& item : map_broadcast)
                        {
                            auto const& file_uri = item.first;
                            auto const& channel_address = item.second.first;

                            if (item.second.second)
                            {
#ifdef EXTRA_LOGGING
                                impl.writeln_node(file_uri + " session_action_get_file_uris callback calling initiate revert");
#endif
                                impl.m_storage_controller.initiate(file_uri, channel_address, storage_controller::revert);
                            }
                        }
                    });

                    for (auto& actions : map_actions)
                    {
                        meshpp::nodeid_session_header header;
                        header.nodeid = actions.first;
                        header.address = map_nodeid_ip_address.at(actions.first);
                        impl.m_nodeid_sessions.add(header,
                                                   std::move(actions.second),
                                                   chrono::minutes(3));
                    }

                    for (auto& item : map_broadcast)
                    {
                        auto const& file_uri = item.first;
                        auto const& channel_address = item.second.first;
#ifdef EXTRA_LOGGING
                        beltpp::on_failure guard([&impl, file_uri]{impl.writeln_node(file_uri + " flew");});
#endif
                        if (false == impl.m_documents.storage_has_uri(file_uri, impl.m_pb_key.to_string()))
                            broadcast_storage_update(impl, file_uri, UpdateType::store);

#ifdef EXTRA_LOGGING
                        impl.writeln_node(file_uri + " session_action_get_file_uris callback calling pop");
#endif
                        impl.m_storage_controller.initiate(file_uri, channel_address, storage_controller::revert);
                        item.second.second = false;
                        impl.m_storage_controller.pop(file_uri, channel_address);
#ifdef EXTRA_LOGGING
                        guard.dismiss();
#endif
                    }
                }
                else
                {
                    if (package.type() == BlockchainMessage::RemoteError::rtt)
                    {
                        BlockchainMessage::RemoteError remote_error;
                        std::move(package).get(remote_error);
#ifdef EXTRA_LOGGING
                        impl.writeln_node(remote_error.message);
#endif
                    }
#ifdef EXTRA_LOGGING
                    else
                    {
                        impl.writeln_node("cannot get the files list - " + package.to_string());
                    }
                    impl.writeln_node("session_action_get_file_uris callback calling initiate revert " + std::to_string(file_to_channel.size()));
#endif
                    for (auto const& item : file_to_channel)
                        impl.m_storage_controller.initiate(item.first, item.second, storage_controller::revert);
                }
            };

            if (false == file_to_channel.empty())
            {
#ifdef EXTRA_LOGGING
                m_pimpl->writeln_node("can download now: " + std::to_string(file_to_channel.size()));
                impl.writeln_node("verified channels: " + std::to_string(map_nodeid_ip_address.size()));
#endif
                vector<unique_ptr<meshpp::session_action<meshpp::session_header>>> actions;
                actions.emplace_back(new session_action_get_file_uris(impl, get_file_uris_callback));

                meshpp::session_header header;
                header.peerid = "slave";
                m_pimpl->m_sessions.add(header,
                                        std::move(actions),
                                        chrono::minutes(1));
            }
        }
    }
}

void node::set_slave_node(storage_node& slave_node)
{
    m_pimpl->m_slave_node = &slave_node;
    m_pimpl->m_slave_node->m_pimpl->m_node_type = m_pimpl->m_node_type;
}

//#define log_log_log

//  free functions
void block_worker(detail::node_internals& impl)
{
    if (impl.all_sync_info.blockchain_sync_in_progress)
        return;

    ///  clean old entries from votes map
    //
    auto const steady_clock_now = std::chrono::steady_clock::now();
    auto vote_iter = impl.m_votes.begin();
    while (vote_iter != impl.m_votes.end())
    {
        auto const& tp = vote_iter->second.tp;
        assert (steady_clock_now >= tp);

        if (steady_clock_now - tp >= std::chrono::seconds(SYNC_TIMER * 2))
            vote_iter = impl.m_votes.erase(vote_iter);
        else
            ++vote_iter;
    }

    ///  update the votes map, to consider latest sync responses
    //
    // get the ip addresses
    unordered_map<string, string> map_nodeid_ip_address;
    for (auto const& peerid : impl.m_p2p_peers)
        map_nodeid_ip_address[peerid] = impl.m_ptr_p2p_socket->info_connection(peerid).remote.address;

    PublicAddressesInfo public_addresses = impl.m_nodeid_service.get_addresses();

    for (auto const& item : public_addresses.addresses_info)
    {
        if (item.seconds_since_checked > PUBLIC_ADDRESS_FRESH_THRESHHOLD_SECONDS)
            break;
        if (impl.m_p2p_peers.count(item.node_address))
            continue;

        map_nodeid_ip_address[item.node_address] = item.ip_address.local.address;
    }

    // use the ip addresses, to keep one vote per ip address
    for (auto const& item : impl.all_sync_info.sync_responses)
    {
        auto it_ip_address = map_nodeid_ip_address.find(item.first);
        if (it_ip_address == map_nodeid_ip_address.end())
            continue;
        string str_ip_address = it_ip_address->second;
        auto& replacing = impl.m_votes[str_ip_address];
        auto voting = detail::node_internals::vote_info{
                          impl.m_state.get_balance(item.first, state_layer::pool),
                          item.second.own_header.block_hash,
                          steady_clock_now};
        if (voting.stake <= replacing.stake)
            continue;
#ifdef  log_log_log
        impl.writeln_node("voting for: " + voting.block_hash + ", voter: " + item.first);
#endif

        replacing = voting;
    }

    struct vote_sum
    {
        coin approve;
        coin reject;
    };

    unordered_map<string, vote_sum> vote_results;

    coin sum_stake;
    size_t poll_participants = 0;
    size_t poll_participants_with_stake = 0;

    for (auto const& vote_item : impl.m_votes)
    {
        auto const& vote = vote_item.second;
        ++poll_participants;
        if (vote.stake != coin())
            ++poll_participants_with_stake;

        auto current_stake = vote.stake + coin(1,0);
        sum_stake += current_stake;

        vote_results[vote.block_hash].approve += current_stake;
    }

    auto own_vote = detail::node_internals::vote_info{
                        impl.m_state.get_balance(impl.m_pb_key.to_string(), state_layer::pool),
                        impl.m_blockchain.last_header_ex().block_hash,
                        steady_clock_now};
    {
        auto current_stake = own_vote.stake + coin(1,0);
        sum_stake += current_stake;

        vote_results[own_vote.block_hash].approve += current_stake;
    }

    for (auto& vote_result_item : vote_results)
    {
        vote_result_item.second.reject = sum_stake - vote_result_item.second.approve;
    }

    uint64_t const poll_participants_with_stake_treshhold =
            false == impl.m_testnet ?
                std::max(uint64_t(2), uint64_t(impl.m_p2p_peers.size() / 4)) :
                1;

    unordered_set<string> least_revert_approved_winner_candidates;
    string scan_most_approved_revert_candidate;
    coin scan_most_approved_revert;

    for (auto& vote_result_item : vote_results)
    {
        auto const& approve = vote_result_item.second.approve;
        auto const& reject = vote_result_item.second.reject;

        if (approve > reject &&
            poll_participants > std::max(uint64_t(2), uint64_t(impl.m_p2p_peers.size() / 3)) &&
            poll_participants_with_stake > poll_participants_with_stake_treshhold)
        {
            least_revert_approved_winner_candidates.insert(vote_result_item.first);
        }

        if (approve > scan_most_approved_revert &&
            approve > own_vote.stake &&
            poll_participants > std::max(uint64_t(10), uint64_t(impl.m_p2p_peers.size() / 3)) &&
            poll_participants_with_stake > poll_participants_with_stake_treshhold)
        {
            scan_most_approved_revert_candidate = vote_result_item.first;
            scan_most_approved_revert = approve;
        }
    }

    auto const blockchain_length = impl.m_blockchain.length();
    auto const last_header = impl.m_blockchain.last_header();

    chrono::system_clock::duration last_block_age =
            chrono::system_clock::now() -
            chrono::system_clock::from_time_t(last_header.time_signed.tm);

    double last_block_age_seconds = double(chrono::duration_cast<chrono::seconds>(last_block_age).count());
    double last_block_age_blocks = last_block_age_seconds / BLOCK_MINE_DELAY;

    double revert_fraction = std::min(1.0, last_block_age_blocks);

    auto it_scan_least_revert = impl.all_sync_info.headers_actions_data.end();
    auto it_scan_least_revert_approved_winner = impl.all_sync_info.headers_actions_data.end();
    auto it_scan_most_approved_revert = impl.all_sync_info.headers_actions_data.end();

    session_action_block::reason reason_scan_least_revert;
    session_action_block::reason reason_scan_least_revert_approved_winner;
    session_action_block::reason reason_scan_most_approved_revert;

    for (auto it = impl.all_sync_info.headers_actions_data.begin();
         it != impl.all_sync_info.headers_actions_data.end();
         ++it)
    {
        if (it->second.headers.empty())
            continue;

        auto scan_peer = it->first;
        auto start_number = it->second.headers.back().block_number;

        if (start_number > blockchain_length)
            continue;   //  in case we had earlier reverted to a shorter chain
                        //  but an even older headers response is laying around

        if (start_number > 0 &&
            impl.m_blockchain.header_ex_at(start_number - 1).block_hash !=
            it->second.headers.back().prev_hash)
            continue;

        double revert_coefficient = 0;
        size_t revert_count = blockchain_length - start_number;
        if (1 <= revert_count)
            revert_coefficient = revert_count - 1 + revert_fraction;

        it->second.reverts_required = revert_coefficient;

        bool unsafe_time_to_revert = BLOCK_SAFE_DELAY < revert_coefficient * BLOCK_MINE_DELAY;
        //  that is if revert_coefficient > 0.4
        //  or in other words need to revert block that is older than 4 minutes

        if (unsafe_time_to_revert)
        {
            if (least_revert_approved_winner_candidates.count(it->second.headers.front().block_hash))
            {
                if (impl.all_sync_info.headers_actions_data.end() != it_scan_least_revert_approved_winner &&
                    it_scan_least_revert_approved_winner->second.headers.front() != it->second.headers.front())
                    impl.writeln_node("two or more absolute majority in voting?");

                if (impl.all_sync_info.headers_actions_data.end() == it_scan_least_revert_approved_winner ||
                    it_scan_least_revert_approved_winner->second.reverts_required > revert_coefficient)
                {
                    it_scan_least_revert_approved_winner = it;
                    reason_scan_least_revert_approved_winner.v = session_action_block::reason::unsafe_best;
                    reason_scan_least_revert_approved_winner.poll_participants = poll_participants;
                    reason_scan_least_revert_approved_winner.poll_participants_with_stake = poll_participants_with_stake;
                    reason_scan_least_revert_approved_winner.revert_coefficient = revert_coefficient;
                }
            }
#ifdef log_log_log
            auto approve = vote_results[it->second.headers.front().block_hash].approve;
            auto reject = vote_results[it->second.headers.front().block_hash].reject;
            impl.writeln_node("\t\ttesting: " + it->second.headers.front().block_hash);
            impl.writeln_node("\t\tfull approve:" + approve.to_string());
            impl.writeln_node("\t\tfull reject:" + reject.to_string());
            impl.writeln_node("\t\tscan_most_approved_revert:" + scan_most_approved_revert.to_string());
#endif

            if (scan_most_approved_revert_candidate == it->second.headers.front().block_hash &&
                (
                    impl.all_sync_info.headers_actions_data.end() == it_scan_most_approved_revert ||
                    it_scan_most_approved_revert->second.reverts_required > revert_coefficient
                )
               )
            {
#ifdef log_log_log
                impl.writeln_node("\tchoosing: " + it->second.headers.front().block_hash);
                impl.writeln_node("\tfull approve:" + approve.to_string());
                impl.writeln_node("\tfull reject:" + reject.to_string());
                impl.writeln_node("\tscan_most_approved_revert:" + scan_most_approved_revert.to_string());
#endif

                it_scan_most_approved_revert = it;
                reason_scan_most_approved_revert.v = session_action_block::reason::unsafe_better;
                reason_scan_most_approved_revert.poll_participants = poll_participants;
                reason_scan_most_approved_revert.poll_participants_with_stake = poll_participants_with_stake;
                reason_scan_most_approved_revert.revert_coefficient = revert_coefficient;
            }
        }
        else if (impl.all_sync_info.headers_actions_data.end() == it_scan_least_revert ||
                 it_scan_least_revert->second.reverts_required > revert_coefficient)
        {
            it_scan_least_revert = it;
            if (revert_coefficient > 0)
            {
                reason_scan_least_revert.v = session_action_block::reason::safe_revert;
                reason_scan_least_revert.poll_participants = 0;
                reason_scan_least_revert.poll_participants_with_stake = 0;
                reason_scan_least_revert.revert_coefficient = revert_coefficient;
            }
            else
            {
                reason_scan_least_revert.v = session_action_block::reason::safe_better;
                reason_scan_least_revert.poll_participants = 0;
                reason_scan_least_revert.poll_participants_with_stake = 0;
                reason_scan_least_revert.revert_coefficient = revert_coefficient;
            }
        }
    }

    session_action_block::reason t_reason;

    auto it_chosen = impl.all_sync_info.headers_actions_data.end();
    if (impl.all_sync_info.headers_actions_data.end() != it_scan_least_revert_approved_winner)
    {
        it_chosen = it_scan_least_revert_approved_winner;
        t_reason = reason_scan_least_revert_approved_winner;
    }
    else if (impl.all_sync_info.headers_actions_data.end() != it_scan_least_revert)
    {
        it_chosen = it_scan_least_revert;
        t_reason = reason_scan_least_revert;
    }
    else if (impl.all_sync_info.headers_actions_data.end() != it_scan_most_approved_revert)
    {
        it_chosen = it_scan_most_approved_revert;
        t_reason = reason_scan_most_approved_revert;
    }
    else
    {
        if (last_block_age_blocks > 3 &&
            impl.m_stuck_on_old_blockchain_timer.expired())
        {
            impl.m_stuck_on_old_blockchain_timer.update();
            impl.writeln_node_warning("has stuck on an old blockchain");
        }
    }

    if (impl.all_sync_info.headers_actions_data.end() != it_chosen)
    {
        vector<unique_ptr<meshpp::session_action<meshpp::nodeid_session_header>>> actions;
        actions.emplace_back(new session_action_block(impl, t_reason));

        meshpp::nodeid_session_header header;
        header.nodeid = it_chosen->first;
        header.address = impl.m_ptr_p2p_socket->info_connection(it_chosen->first);
        impl.m_sync_sessions.add(header,
                                 std::move(actions),
                                 chrono::seconds(SYNC_TIMER));
    }
}

double header_worker(detail::node_internals& impl)
{
    block_worker(impl);

    // process collected SyncResponse data
    BlockHeaderExtended const head_block_header = impl.m_blockchain.last_header_ex();

    BlockHeaderExtended scan_block_header = head_block_header;
    scan_block_header.c_sum = 0;

    double revert_coefficient = 0;
    beltpp::isocket::peer_id scan_peer;
    unordered_set<string> requested_blocks_hashes;

    for (auto const& item : impl.all_sync_info.sync_responses)
    {
        if (0 == impl.m_p2p_peers.count(item.first))
            continue; // for the case if peer is dropped before sync started, or this is a public contact (using rpc)

        auto it = impl.all_sync_info.headers_actions_data.find(item.first);
        if (it != impl.all_sync_info.headers_actions_data.end() && it->second.reverts_required > 0)
        {
            revert_coefficient = it->second.reverts_required;
            requested_blocks_hashes.insert(item.second.own_header.block_hash);
            continue; // don't consider this peer during header action is active
        }

        if (head_block_header.block_hash != item.second.own_header.block_hash &&
            scan_block_header.c_sum < item.second.own_header.c_sum &&
            scan_block_header.block_number <= item.second.own_header.block_number &&
            requested_blocks_hashes.count(item.second.own_header.block_hash) == 0)
        {
            scan_block_header = item.second.own_header;
            scan_peer = item.first;
        }
    }

    //  work through process of block header sync or mining
    //  if there is no active sync
    if (false == impl.all_sync_info.blockchain_sync_in_progress)
    {
        //  the duration passed according to my system time since the head block
        //  was signed
        chrono::system_clock::duration since_head_block =
                system_clock::now() -
                system_clock::from_time_t(head_block_header.time_signed.tm);

        bool mine_now = false;
        bool sync_now = false;

        auto net_sum = impl.all_sync_info.net_sync_info().c_sum;
        auto own_sum = impl.all_sync_info.own_sync_info().c_sum;

        if (head_block_header.block_number + 1 < scan_block_header.block_number &&
            // maybe need to move the following check right inside sync responce handler?
            chrono::seconds(BLOCK_MINE_DELAY + BLOCK_WAIT_DELAY) <= since_head_block)
        {
            // network is far ahead and I have to sync first
            sync_now = true;
        }
        else if (head_block_header.c_sum < scan_block_header.c_sum &&
                 head_block_header.block_number == scan_block_header.block_number)
        {
            //  there is a better consensus sum than what I have and I must get it first
            sync_now = true;
        }
        else if (since_head_block < chrono::seconds(BLOCK_SAFE_DELAY) &&
                 head_block_header.c_sum == scan_block_header.c_sum &&
                 head_block_header.block_number == scan_block_header.block_number &&
                 head_block_header.block_hash != scan_block_header.block_hash)
        {
            // there can be a normal fork and I am trying go to the best miner
            sync_now = true;
        }
        else if (own_sum < scan_block_header.c_sum &&
                 net_sum < scan_block_header.c_sum)
        {
            //  direct peer has ready excellent chain to offer
            sync_now = true;
        }
        else if (own_sum < scan_block_header.c_sum &&
                 net_sum == scan_block_header.c_sum)
        {
            //  direct peer finally got the excellent chain that was promised before
            sync_now = true;
        }
        else if (own_sum < scan_block_header.c_sum/* &&
                 net_sum > scan_block_header.c_sum*/)
        {
            //  the promised excellent block did not arrive yet
            //  and the network has ready better block than I can mine
            if (chrono::seconds(BLOCK_MINE_DELAY + BLOCK_WAIT_DELAY) < since_head_block)
            {
                //  and it is too late already, can't wait anymore
                sync_now = true;
            }
            //  otherwise will just wait
        }
        else// if (own_sum >= scan_block_header.c_sum)
        {
            //  I can mine better block and don't need received data
            //  or I have something better

            //  wait until
            if (chrono::seconds(BLOCK_MINE_DELAY - BLOCK_WAIT_DELAY) <= since_head_block &&
                since_head_block < chrono::seconds(BLOCK_MINE_DELAY))
            {
                //  last two minutes before another block will be added
                //  check if there is a fork
                if (false == scan_peer.empty())
                    sync_now = true;
            }
            else if (chrono::seconds(BLOCK_MINE_DELAY + BLOCK_WAIT_DELAY) <= since_head_block ||
                     (chrono::seconds(BLOCK_MINE_DELAY) <= since_head_block && own_sum >= net_sum))
            {
                //  either BLOCK_MINE_DELAY has passed and I can mine better than scan_consensus_sum
                //  or BLOCK_MINE_DELAY + BLOCK_WAIT_DELAY has passed and I can mine better than net_sum
                mine_now = true;
                assert(false == sync_now);
            }
        }

        assert(false == sync_now || false == mine_now);

        if (sync_now)
        {
            assert(false == scan_peer.empty());
            if (scan_peer.empty())
                throw std::logic_error("scan_peer.empty()");

            vector<unique_ptr<meshpp::session_action<meshpp::nodeid_session_header>>> actions;
            actions.emplace_back(new session_action_p2pconnections(*impl.m_ptr_p2p_socket.get()));
            actions.emplace_back(new session_action_sync_request(impl,
                                                                 *impl.m_ptr_p2p_socket.get()));
            actions.emplace_back(new session_action_header(impl,
                                                           scan_block_header));

            meshpp::nodeid_session_header header;
            header.nodeid = scan_peer;
            header.address = impl.m_ptr_p2p_socket->info_connection(scan_peer);
            impl.m_sync_sessions.add(header,
                                     std::move(actions),
                                     chrono::seconds(SYNC_TIMER));
        }
        else if (impl.is_miner() && mine_now)
        {
            mine_block(impl);
        }
    }

    return revert_coefficient;
}

void sync_worker(detail::node_internals& impl)
{
    double revert_coefficient = header_worker(impl);

    for (auto const& peerid : impl.m_p2p_peers)
    {
        vector<unique_ptr<meshpp::session_action<meshpp::nodeid_session_header>>> actions;
        actions.emplace_back(new session_action_p2pconnections(*impl.m_ptr_p2p_socket.get()));
        actions.emplace_back(new session_action_sync_request(impl,
                                                             *impl.m_ptr_p2p_socket.get()));

        meshpp::nodeid_session_header header;
        header.nodeid = peerid;
        header.address = impl.m_ptr_p2p_socket->info_connection(peerid);
        impl.m_sync_sessions.add(header,
                                 std::move(actions),
                                 chrono::seconds(SYNC_TIMER));
    }

    if (0 < revert_coefficient)
    {
        PublicAddressesInfo public_addresses = impl.m_nodeid_service.get_addresses();

        for (auto const& item : public_addresses.addresses_info)
        {
            if (item.seconds_since_checked > PUBLIC_ADDRESS_FRESH_THRESHHOLD_SECONDS)
                break;
            if (impl.m_p2p_peers.count(item.node_address))
                continue;
            NodeType node_type;
            if (impl.m_state.get_role(item.node_address, node_type))
                continue;
            if (impl.m_pb_key.to_string() == item.node_address)
                continue;

            vector<unique_ptr<meshpp::session_action<meshpp::nodeid_session_header>>> actions;
            actions.emplace_back(new session_action_connections(*impl.m_ptr_rpc_socket.get()));
            actions.emplace_back(new session_action_signatures(*impl.m_ptr_rpc_socket.get(),
                                                               impl.m_nodeid_service));
            actions.emplace_back(new session_action_sync_request(impl,
                                                                 *impl.m_ptr_rpc_socket.get()));

            meshpp::nodeid_session_header header;
            header.nodeid = item.node_address;
            beltpp::assign(header.address, item.ip_address);
            impl.m_nodeid_sessions.add(header,
                                       std::move(actions),
                                       chrono::seconds(SYNC_TIMER));
        }
    }
}
}


