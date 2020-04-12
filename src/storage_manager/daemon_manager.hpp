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
class sm_sync_context_internals;
}

class sm_daemon;

class sm_sync_context
{
public:
    sm_sync_context(manager& sm_server, std::string const& address);
    sm_sync_context(manager& sm_server, sm_daemon& sm_daemon);
    sm_sync_context(sm_sync_context&&);
    ~sm_sync_context();
    uint64_t start_index() const;
    void save();
    void commit();

    std::unique_ptr<::detail::sm_sync_context_internals> m_pimpl;
};

class sm_daemon
{
public:
    sm_daemon();

    void open(beltpp::ip_address const& connect_to_address);
    void close();

    sm_sync_context start_sync(manager& sm_server);
    sm_sync_context start_import(manager& sm_server, std::string const& address);
    
    void sync(manager& sm_server, sm_sync_context& context);
    beltpp::packet wait_response(std::string const& transaction_hash);

    beltpp::event_handler_ptr eh;
    beltpp::socket_ptr socket;
    beltpp::stream::peer_id peerid;
    meshpp::file_loader<ManagerMessage::NumberValue, &ManagerMessage::NumberValue::from_string, &ManagerMessage::NumberValue::to_string> log_index;
};
