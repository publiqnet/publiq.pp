#pragma once

#include "commander_message.hpp"

#include <publiq.pp/message.hpp>
#include <publiq.pp/message.tmpl.hpp>

#include <belt.pp/timer.hpp>
#include <belt.pp/ievent.hpp>
#include <belt.pp/socket.hpp>

#include <mesh.pp/fileutility.hpp>

#include <unordered_map>

class rpc
{
public:
    rpc(std::string const& str_pv_key,
        beltpp::ip_address const& rpc_address,
        beltpp::ip_address const& connect_to_address,
        uint64_t sync_interval);

    void run();

    std::string m_str_pv_key;
    beltpp::event_handler_ptr eh;
    beltpp::socket_ptr rpc_socket;
    meshpp::file_loader<CommanderMessage::NumberValue, &CommanderMessage::NumberValue::from_string, &CommanderMessage::NumberValue::to_string> head_block_index;
    meshpp::map_loader<CommanderMessage::Account> accounts;
    meshpp::vector_loader<CommanderMessage::BlockInfo> blocks;
    meshpp::map_loader<CommanderMessage::StoragesResponseItem> storages;
    meshpp::map_loader<CommanderMessage::ChannelsResponseItem> channels;

    beltpp::ip_address const& connect_to_address;

    beltpp::timer m_storage_update_timer;
    std::unordered_map<uint64_t, std::unordered_map<std::string, uint64_t>> m_file_usage_map;
    std::unordered_map<std::string, std::pair<std::string, std::string>> m_file_location_map;
};

inline
void range_break(uint64_t rw_min, uint64_t rw_max,
                 uint64_t tx_min, uint64_t tx_max,
                 uint64_t& rw1_min, uint64_t& rw1_max,
                 uint64_t& tx1_min, uint64_t& tx1_max,
                 uint64_t& rwtx_min, uint64_t& rwtx_max,
                 uint64_t& rw2_min, uint64_t& rw2_max,
                 uint64_t& tx2_min, uint64_t& tx2_max)
{
    rw1_min = 0; rw1_max = 0;
    tx1_min = 0; tx1_max = 0;
    rwtx_min = 0; rwtx_max = 0;
    rw2_min = 0; rw2_max = 0;
    tx2_min = 0; tx2_max = 0;

    if (rw_max <= tx_min)
    {
        rw1_min = rw_min;
        rw1_max = rw_max;

        tx2_min = tx_min;
        tx2_max = tx_max;
    }
    else if (tx_max <= rw_min)
    {
        tx1_min = tx_min;
        tx1_max = tx_max;

        rw2_min = rw_min;
        rw2_max = rw_max;
    }
    else
    {
        if (rw_min < tx_min)
        {
            rw1_min = rw_min;
            rw1_max = tx_min;
        }
        if (tx_min < rw_min)
        {
            tx1_min = tx_min;
            tx1_max = rw_min;
        }
        if (rw_max > tx_max)
        {
            rw2_min = tx_max;
            rw2_max = rw_max;
        }
        if (tx_max > rw_max)
        {
            tx2_min = rw_max;
            tx2_max = tx_max;
        }

        rwtx_min = std::max(rw_min, tx_min);
        rwtx_max = std::min(rw_max, tx_max);
    }
}
