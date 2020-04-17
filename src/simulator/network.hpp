#pragma once

#include <belt.pp/isocket.hpp>
#include <belt.pp/ievent.hpp>
#include <belt.pp/queue.hpp>
#include <belt.pp/timer.hpp>
#include <belt.pp/packet.hpp>

#include <chrono>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <vector>
#include <list>
#include <exception>
#include <string>

using std::map;
using std::pair;
using std::list;
using std::vector;

using beltpp::packet;
using beltpp::socket;
using beltpp::ip_address;
using beltpp::event_item;
using beltpp::event_handler;

namespace network_simulation_impl
{
class socket_ns;
class event_handler_ns;

class network_simulation
{
public:
    using peer_type = beltpp::socket::peer_type;

    struct ip_cmp
    {
        bool operator()(const ip_address& a, const ip_address& b) const
        {
            return a.to_string() < b.to_string();
        }
    };

    //  from_addr       to_addr     packets
    map<ip_address, map<ip_address, list<packet>, ip_cmp>, ip_cmp> send_receive_status;


    network_simulation();
    ~network_simulation();

    void add_handler(               event_handler_ns& eh);

    void remove_handler(            event_handler_ns& eh);

//    void add_socket(                event_handler_ns& eh,
//                                    beltpp::event_item& ev_it);
//
//    void remove_socket(             event_handler_ns& eh,
//                                    beltpp::event_item& ev_it);
//
//    bool check_socket(              event_handler_ns& eh,
//                                    beltpp::event_item& ev_it);
//
//    void add_connection(            event_handler_ns& eh,
//                                    beltpp::event_item& ev_it,
//                                    beltpp::ip_address addres);
//
//    bool remove_connection(         event_handler_ns& eh,
//                                    beltpp::event_item& ev_it,
//                                    beltpp::ip_address address);
//
//    bool check_concection(          event_handler_ns& eh,
//                                    beltpp::event_item& ev_it,
//                                    beltpp::ip_address address);
//
//    bool change_connection_status(  event_handler_ns& eh,
//                                    beltpp::event_item& ev_it,
//                                    beltpp::ip_address address,
//                                    connection_status status);

    void send_packet(               event_handler_ns& eh,
                                    //beltpp::event_item& ev_it,
                                    beltpp::ip_address to_address,
                                    beltpp::packet const& packets);

    //    void receive_packet(            event_handler_ns& eh,
//                                    beltpp::event_item& ev_it,
//                                    beltpp::ip_address address,
//                                    beltpp::socket::packets& packets);
//
//    bool check_packets(             event_handler_ns& eh,
//                                    std::unordered_set<beltpp::event_item const*>& set_items);
};

class event_handler_ns : public beltpp::event_handler
{
public:
    event_handler_ns(network_simulation& ns);
    ~event_handler_ns() override;

    wait_result wait(std::unordered_set<beltpp::event_item const*>& set_items) override;
    std::unordered_set<uint64_t> waited(beltpp::event_item& ev_it) const override;

    void wake() override;
    void set_timer(std::chrono::steady_clock::duration const& period) override;

    void add(beltpp::event_item& ev_it) override;
    void remove(beltpp::event_item& ev_it) override;

    network_simulation* m_ns;
    beltpp::timer m_timer_helper;
};

class socket_ns : public beltpp::socket
{
public:
    using peer_id = socket::peer_id;
    using peer_ids = socket::peer_ids;

    socket_ns(event_handler_ns& eh);
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

    std::string dump() const override;

private:
    event_handler_ns* m_eh;
    network_simulation* m_ns;

    beltpp::detail::session_special_data temp_special_data;
};


beltpp::ip_address peer_to_address(beltpp::socket::peer_id id);
beltpp::socket::peer_id address_to_peer(beltpp::ip_address address);

}// network_simulation_impl


