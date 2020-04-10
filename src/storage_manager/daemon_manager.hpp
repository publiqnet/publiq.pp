#pragma once

#include "storage_manager_message.hpp"

#include <belt.pp/ievent.hpp>
#include <belt.pp/socket.hpp>

#include <mesh.pp/fileutility.hpp>

#include <publiq.pp/message.hpp>
#include <publiq.pp/message.tmpl.hpp>

#include <utility>
#include <unordered_set>
#include <unordered_map>
#include <string>

class manager;

namespace detail
{
class sm_sync_context_detail;
}

class sm_daemon;

class sm_sync_context
{
public:
    using TransactionLogLoader = meshpp::vector_loader<BlockchainMessage::TransactionLog>;
    using LogIndexLoader = meshpp::map_loader<ManagerMessage::NumberPair>;

    sm_sync_context(manager& sm_server, std::string const& account);
    sm_sync_context(manager& sm_server, sm_daemon& sm_daemon, std::unordered_set<std::string> const& set_accounts);
    sm_sync_context(sm_sync_context&&);
    ~sm_sync_context();
    uint64_t start_index() const;
    void save();
    void commit();

    TransactionLogLoader& transactions(std::string const& account);
    LogIndexLoader& index_transactions(std::string const& account);

    std::unique_ptr<::detail::sm_sync_context_detail> m_pimpl;
};

class sm_daemon
{
public:
    sm_daemon();

    void open(beltpp::ip_address const& connect_to_address);
    void close();

    static
    sm_sync_context::TransactionLogLoader get_transaction_log(std::string const& address);
    static
    sm_sync_context::LogIndexLoader get_transaction_log_index(std::string const& address);

    beltpp::packet process_storage_update_request(ManagerMessage::StorageUpdateRequest const& update, manager& sm_server);

    beltpp::packet send(ManagerMessage::Send const& send, manager& sm_server);

    beltpp::packet wait_response(std::string const& transaction_hash);

    sm_sync_context start_new_import(manager& sm_server, std::string const& account);

    sm_sync_context start_sync(manager& sm_server, std::unordered_set<std::string> const& set_accounts);

    void sync(manager& sm_server, sm_sync_context& context);

    beltpp::event_handler_ptr eh;
    beltpp::socket_ptr socket;
    beltpp::stream::peer_id peerid;
    meshpp::file_loader<ManagerMessage::NumberValue, &ManagerMessage::NumberValue::from_string, &ManagerMessage::NumberValue::to_string> log_index;
};
