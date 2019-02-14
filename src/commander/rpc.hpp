#pragma once

#include "message.hpp"

#include <belt.pp/socket.hpp>
#include <mesh.pp/fileutility.hpp>

class rpc
{
public:
    rpc(beltpp::ip_address const& rpc_address,
        beltpp::ip_address const& connect_to_address);

    void run();

    beltpp::event_handler eh;
    beltpp::socket rpc_socket;
    meshpp::file_loader<CommanderMessage::NumberValue, &CommanderMessage::NumberValue::from_string, &CommanderMessage::NumberValue::to_string> head_block_index;
    meshpp::map_loader<CommanderMessage::Account> accounts;
    beltpp::ip_address const& connect_to_address;
};
