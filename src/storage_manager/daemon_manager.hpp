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

class sm_daemon
{
public:
    sm_daemon(manager& server);

    void open(beltpp::ip_address const& connect_to_address);
    void close();

    void sync();
    void save();
    void commit();

    beltpp::packet wait_response(std::string const& transaction_hash);

    manager& sm_server;
    beltpp::event_handler_ptr eh;
    beltpp::socket_ptr socket;
    beltpp::stream::peer_id peerid;
    meshpp::file_loader<ManagerMessage::NumberValue, &ManagerMessage::NumberValue::from_string, &ManagerMessage::NumberValue::to_string> log_index;

    beltpp::on_failure m_guard;
};
