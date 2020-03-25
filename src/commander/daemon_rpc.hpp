#pragma once

#include "commander_message.hpp"

#include <belt.pp/socket.hpp>
#include <belt.pp/event.hpp>

#include <mesh.pp/fileutility.hpp>

#include <publiq.pp/message.hpp>
#include <publiq.pp/message.tmpl.hpp>

#include <utility>
#include <unordered_set>
#include <unordered_map>
#include <string>

class rpc;

namespace detail
{
class sync_context_detail;
}

class daemon_rpc;
class sync_context
{
public:
    using TransactionLogLoader = meshpp::vector_loader<BlockchainMessage::TransactionLog>;
    using RewardLogLoader = meshpp::vector_loader<BlockchainMessage::RewardLog>;
    using LogIndexLoader = meshpp::map_loader<CommanderMessage::NumberPair>;

    sync_context(rpc& ref_rpc_server, std::string const& account);
    sync_context(rpc& ref_rpc_server, daemon_rpc& ref_daemon_rpc, std::unordered_set<std::string> const& set_accounts);
    sync_context(sync_context&&);
    ~sync_context();
    uint64_t start_index() const;
    void save();
    void commit();

    TransactionLogLoader& transactions(std::string const& account);
    RewardLogLoader& rewards(std::string const& account);
    LogIndexLoader& index_transactions(std::string const& account);
    LogIndexLoader& index_rewards(std::string const& account);

    std::unique_ptr<detail::sync_context_detail> m_pimpl;
};

class daemon_rpc
{
public:
    daemon_rpc();

    void open(beltpp::ip_address const& connect_to_address);
    void close();

    static
    sync_context::TransactionLogLoader get_transaction_log(std::string const& address);
    static
    sync_context::RewardLogLoader get_reward_log(std::string const& address);
    static
    sync_context::LogIndexLoader get_transaction_log_index(std::string const& address);
    static
    sync_context::LogIndexLoader get_reward_log_index(std::string const& address);

    beltpp::packet process_storage_update_request(CommanderMessage::StorageUpdateRequest const& update,
                                                  rpc& rpc_server);
    beltpp::packet send(CommanderMessage::Send const& send,
                        rpc& rpc_server);
    beltpp::packet wait_response(std::string const& transaction_hash);

    sync_context start_new_import(rpc& rpc_server,
                                  std::string const& account);
    sync_context start_sync(rpc& rpc_server,
                            std::unordered_set<std::string> const& set_accounts);

    void sync(rpc& rpc_server, sync_context& context);

    beltpp::event_handler eh;
    beltpp::socket socket;
    beltpp::isocket::peer_id peerid;
    meshpp::file_loader<CommanderMessage::NumberValue, &CommanderMessage::NumberValue::from_string, &CommanderMessage::NumberValue::to_string> log_index;
};
