#pragma once

#include "global.hpp"
#include <publiq.pp/message.hpp>
#include <publiq.pp/message.tmpl.hpp>

#include <mesh.pp/src/libp2psocket/message.hpp>

#include <belt.pp/isocket.hpp>
#include <belt.pp/ievent.hpp>
#include <belt.pp/queue.hpp>
#include <belt.pp/timer.hpp>
#include <belt.pp/packet.hpp>
#include <belt.pp/utility.hpp>

#include <chrono>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <set>
#include <vector>
#include <list>
#include <exception>
#include <string>
#include <iomanip>
#include <sstream>
#include <mutex>

using std::map;
using std::set;
using std::pair;
using std::list;
using std::string;
using std::unordered_set;

using beltpp::packet;
using beltpp::socket;
using beltpp::ip_address;
using beltpp::event_item;
using beltpp::event_handler;
using beltpp::ip_destination;

using peer_id = beltpp::socket::peer_id;
using peer_ids = beltpp::socket::peer_ids;
using peer_type = beltpp::socket::peer_type;

std::mutex g_mutex;

inline string format_index(size_t node_index, size_t node_count, char c = '0')
{
    size_t base = 0;
    while (node_count > 0)
    {
        ++base;
        node_count /= 10;
    }

    std::stringstream ss;
    ss << std::setw(base) << std::setfill(c) << node_index;
    return ss.str();
}

namespace network_simulation_impl
{

class socket_ns;
class event_handler_ns;

class SIMULATORSHARED_EXPORT network_simulation
{
public:

    struct ip_cmp
    {
        bool operator()(const ip_address& a, const ip_address& b) const
        {
            return a.to_string() < b.to_string();
        }
    };

    struct ip_dest_cmp
    {
        bool operator()(const ip_destination& a, const ip_destination& b) const
        {
            return a.address + ":" + std::to_string(a.port) < b.address + ":" + std::to_string(b.port);
        }
    };

    //receiver      sender       packets
    map<string, map<peer_id, list<packet>>> receive_send;

    map<string, map<string, size_t>> receive_send_counter;

    map<ip_address, peer_id, ip_cmp> open_attempts;
    map<ip_destination, pair<peer_id, ip_address>, ip_dest_cmp> listen_attempts;
    
    set<peer_id> peers_to_drop;

    map<peer_id, peer_id> peer_to_peer;
    map<peer_id, ip_address> peer_to_ip;
    map<peer_id, string> peer_to_socket;

    map<event_item*, string> socket_to_name;
    map<string, pair<event_item*, event_item*>> name_to_sockets;
    map<event_handler_ns*, unordered_set<string>> eh_to_sockets;

    map<string, set<string>> permanent_allowed_connections;
    map<string, set<string>> permanent_refused_connections;

    size_t node_count = 0;
    size_t timer_shuffle = 0;
    size_t connection_index = 0;
    uint32_t chance_of_connect_base = 10;

    void process_attempts();
    bool connection_closed(size_t const packet_type) const;
    string construct_peer_id(ip_address const& socket_bundle);
    string export_connections(string socket_name = string());
    string export_connections_matrix();
    string export_connections_load();
    string export_connections_info();
    string export_packets(const size_t rtt = -1);
    string export_network();

    string export_counter();
    void process_counter_state(string const& receiver, string const& sender, bool connect);

private:
    size_t active_connections_count();
    size_t triangle_connections_count();
};

class SIMULATORSHARED_EXPORT event_handler_ns : public beltpp::event_handler
{
public:
    event_handler_ns(network_simulation& ns);
    ~event_handler_ns() override;

    bool read();

    wait_result wait(std::unordered_set<beltpp::event_item const*>& event_items) override;
    std::unordered_set<uint64_t> waited(beltpp::event_item& ev_it) const override;

    void wake() override;
    void set_timer(std::chrono::steady_clock::duration const& period) override;

    void add(beltpp::event_item& ev_it) override;
    void remove(beltpp::event_item& ev_it) override;

    string last_socket_name;
    network_simulation* m_ns;
    bool m_wake_triggered;
    beltpp::timer m_timer_helper;
};

class SIMULATORSHARED_EXPORT socket_ns : public beltpp::socket
{
public:

    socket_ns(event_handler_ns& eh, string& address, string name);
    ~socket_ns() override;

    peer_ids listen(beltpp::ip_address const& address, int backlog = 100) override;

    peer_ids open(beltpp::ip_address address, size_t attempts = 0) override;

    void prepare_wait() override;

    packets receive(peer_id& peer) override;

    void send(peer_id const& peer, beltpp::packet&& pack) override;

    void timer_action() override;

    beltpp::socket::peer_type get_peer_type(peer_id const& peer) override;
    beltpp::ip_address info(peer_id const& peer) override;
    beltpp::ip_address info_connection(peer_id const& peer) override;
    beltpp::detail::session_special_data& session_data(peer_id const& peer) override;

    string dump() const override;

    string m_name; // unique identifier
    string m_address; // local address
    network_simulation* m_ns;
    beltpp::detail::session_special_data temp_special_data;
};

}// network_simulation_impl
