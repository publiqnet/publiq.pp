#pragma once

#include "common.hpp"
#include "http.hpp"

#include "state.hpp"
#include "storage.hpp"
#include "action_log.hpp"
#include "blockchain.hpp"
#include "node.hpp"

#include <belt.pp/event.hpp>
#include <belt.pp/socket.hpp>
#include <belt.pp/packet.hpp>
#include <belt.pp/utility.hpp>
#include <belt.pp/scope_helper.hpp>
#include <belt.pp/message_global.hpp>
#include <belt.pp/timer.hpp>

#include <mesh.pp/p2psocket.hpp>
#include <mesh.pp/cryptoutility.hpp>

#include <boost/filesystem/path.hpp>

#include <map>
#include <chrono>
#include <memory>
#include <list>
#include <utility>
#include <mutex>
#include <unordered_set>

using namespace BlockchainMessage;
namespace filesystem = boost::filesystem;

using beltpp::ip_address;
using beltpp::ip_destination;
using beltpp::socket;
using beltpp::packet;
using peer_id = socket::peer_id;

using std::map;
using std::pair;
using std::string;
using std::list;
using std::unique_ptr;
using std::unordered_set;
using std::unordered_map;

namespace chrono = std::chrono;
using chrono::system_clock;
using chrono::steady_clock;

namespace publiqpp
{
    using rpc_storage_sf = beltpp::socket_family_t<&http::message_list_load<&BlockchainMessage::message_list_load>>;

namespace detail
{

class storage_node_internals
{
public:
    storage_node_internals(
        node& master_node,
        ip_address const& rpc_bind_to_address,
        filesystem::path const& fs_storage,
        meshpp::private_key const& pv_key,
        beltpp::ilog* _plogger_storage_node)
        : m_master_node(&master_node)
        , plogger_storage_node(_plogger_storage_node)
        , m_node_type(NodeType::blockchain)
        , m_ptr_eh(new beltpp::event_handler())
        , m_ptr_rpc_socket(new beltpp::socket(
                               beltpp::getsocket<rpc_storage_sf>(*m_ptr_eh)
                               ))
        , m_rpc_bind_to_address(rpc_bind_to_address)
        , m_storage(fs_storage)
        , m_pv_key(pv_key)
    {
        m_ptr_eh->set_timer(chrono::seconds(EVENT_TIMER));

        if (false == rpc_bind_to_address.local.empty())
            m_ptr_rpc_socket->listen(rpc_bind_to_address);

        m_ptr_eh->add(*m_ptr_rpc_socket);
    }

    void writeln_node(string const& value)
    {
        if (plogger_storage_node)
            plogger_storage_node->message(value);
    }

    void writeln_node_warning(string const& value)
    {
        if (plogger_storage_node)
            plogger_storage_node->warning(value);
    }

    wait_result_item wait_and_receive_one()
    {
        auto& wait_result = m_wait_result.m_wait_result;

        if (wait_result == beltpp::event_handler::wait_result::nothing)
        {
            assert(m_wait_result.on_demand_packets.empty());
            if (false == m_wait_result.on_demand_packets.empty())
                throw std::logic_error("false == m_wait_result.on_demand_packets.empty()");
            assert(m_wait_result.event_packets.empty());
            if (false == m_wait_result.event_packets.empty())
                throw std::logic_error("false == m_wait_result.event_packets.empty()");

            unordered_set<beltpp::ievent_item const*> wait_sockets;

            wait_result = m_ptr_eh->wait(wait_sockets);

            if (wait_result & beltpp::event_handler::event)
            {
                for (auto& pevent_item : wait_sockets)
                {
                    B_UNUSED(pevent_item);
                    wait_result_item::interface_type it = wait_result_item::interface_type::rpc;
                    beltpp::isocket* psk = m_ptr_rpc_socket.get();

                    beltpp::socket::packets received_packets;
                    beltpp::socket::peer_id peerid;
                    received_packets = psk->receive(peerid);

                    auto insert_res = m_wait_result.event_packets.insert({it, std::make_pair(peerid, std::move(received_packets))});
                    assert(insert_res.second);
                    if (false == insert_res.second)
                        throw std::logic_error("auto insert_res = m_wait_result.event_packets.insert({it, std::move(received_packets)});");
                }
            }

            /*if (wait_result & beltpp::event_handler::timer_out)
            {
            }*/

            /*if (wait_result & beltpp::event_handler::on_demand)
            {

            }*/
        }

        auto result = wait_result_item::empty_result();

        if (wait_result & beltpp::event_handler::event)
        {
            if (false == m_wait_result.event_packets.empty())
            {
                auto it = m_wait_result.event_packets.begin();

                if (false == it->second.second.empty())
                {
                    auto interface_type = it->first;
                    auto packet = std::move(it->second.second.front());
                    auto peerid = it->second.first;

                    it->second.second.pop_front();

                    result = wait_result_item::event_result(interface_type, peerid, std::move(packet));
                }

                if (it->second.second.empty())
                    m_wait_result.event_packets.erase(it);
            }

            if (m_wait_result.event_packets.empty())
                wait_result = beltpp::event_handler::wait_result(wait_result & ~beltpp::event_handler::event);

            return result;
        }

        if (wait_result & beltpp::event_handler::timer_out)
        {
            wait_result = beltpp::event_handler::wait_result(wait_result & ~beltpp::event_handler::timer_out);
            result = wait_result_item::timer_result();
            return result;
        }

        if (wait_result & beltpp::event_handler::on_demand)
        {
            result = wait_result_item::on_demand_result(beltpp::packet());

            if (m_wait_result.on_demand_packets.empty())
                wait_result = beltpp::event_handler::wait_result(wait_result & ~beltpp::event_handler::on_demand);

            return result;
        }

        return result;
    }

    node* m_master_node;
    beltpp::ilog* plogger_storage_node;
    NodeType m_node_type;
    unique_ptr<beltpp::event_handler> m_ptr_eh;
    unique_ptr<beltpp::socket> m_ptr_rpc_socket;

    beltpp::ip_address m_rpc_bind_to_address;
    publiqpp::storage m_storage;
    meshpp::private_key m_pv_key;

    std::mutex m_messages_mutex;
    list<pair<beltpp::packet, beltpp::packet>> m_messages;

    unordered_set<string> m_verified_channels;
    wait_result m_wait_result;
};

}
}
