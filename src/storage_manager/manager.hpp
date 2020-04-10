#pragma once

#include "storage_manager_message.hpp"

#include <publiq.pp/message.hpp>
#include <publiq.pp/message.tmpl.hpp>

#include <belt.pp/timer.hpp>
#include <belt.pp/ievent.hpp>
#include <belt.pp/socket.hpp>

#include <mesh.pp/fileutility.hpp>

#include <unordered_map>

class manager
{
public:
    manager(std::string const& str_pv_key,
            beltpp::ip_address const& rpc_address,
            beltpp::ip_address const& connect_to_address,
            uint64_t sync_interval);

    void run();

    std::string str_pv_key;
    beltpp::event_handler_ptr eh;
    beltpp::socket_ptr rpc_socket;

    meshpp::file_loader<ManagerMessage::NumberValue, &ManagerMessage::NumberValue::from_string, &ManagerMessage::NumberValue::to_string> head_block_index;
    meshpp::map_loader<ManagerMessage::FileInfo> files;
    meshpp::map_loader<ManagerMessage::StringValue> storages;

    beltpp::timer storage_update_timer;
    beltpp::ip_address const& connect_to_address;

    std::unordered_map<uint64_t, std::unordered_map<std::string, uint64_t>> file_usage_map;
    std::unordered_map<std::string, std::pair<std::string, std::string>> file_location_map;
};
