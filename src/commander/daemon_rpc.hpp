#pragma once

#include "message.hpp"

#include <belt.pp/socket.hpp>

#include <mesh.pp/fileutility.hpp>

#include <utility>
#include <unordered_set>
#include <string>

class rpc;

class daemon_rpc
{
public:
    daemon_rpc();

    void open(beltpp::ip_address const& connect_to_address);
    void close();

    void sync(rpc& rpc_server,
              std::unordered_set<std::string> const& set_accounts,
              bool const new_import);

    beltpp::event_handler eh;
    beltpp::socket socket;
    beltpp::isocket::peer_id peerid;
    meshpp::file_loader<CommanderMessage::NumberValue, &CommanderMessage::NumberValue::from_string, &CommanderMessage::NumberValue::to_string> log_index;
};
