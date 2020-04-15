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

using beltpp::event_handler;
using beltpp::event_item;
using beltpp::socket;
using beltpp::ip_address;
using beltpp::packet;

namespace simulator_network_impl
{

class event_handler_ns;

class network_simulation
{
public:

    enum class connection_status    {connection_listen, connection_open};
    enum class packet_status        {received, sent};

    using packs         = std::list< std::pair< beltpp::packet, packet_status > >;
    using connection    = std::pair< beltpp::ip_address, connection_status >;
    using connections   = std::vector< std::map< connection, packs > >;
    using sockets       = std::map< beltpp::event_item, connections >;

    std::map< event_handler_ns, sockets > network_status;

    network_simulation();
    ~network_simulation();

    void add_handler(               event_handler_ns& eh);

    void remove_handler(            event_handler_ns& eh);

    void add_socket(                event_handler_ns& eh,
                                    beltpp::event_item& ev_it);

    void remove_socket(             event_handler_ns& eh,
                                    beltpp::event_item& ev_it);

//    bool check_socket(              event_handler_ns& eh,
//                                    beltpp::event_item& ev_it);

    void add_connection(            event_handler_ns& eh,
                                    beltpp::event_item& ev_it,
                                    beltpp::ip_address addres);

//    bool remove_connection(         event_handler_ns& eh,
//                                    beltpp::event_item& ev_it,
//                                    beltpp::ip_address address);

//    bool check_concection(          event_handler_ns& eh,
//                                    beltpp::event_item& ev_it,
//                                    beltpp::ip_address address);

    bool change_connection_status(  event_handler_ns& eh,
                                    beltpp::event_item& ev_it,
                                    beltpp::ip_address address,
                                    connection_status status);

    void send_packet(               event_handler_ns& eh,
                                    beltpp::event_item& ev_it,
                                    beltpp::ip_address address,
                                    beltpp::packet const& packets);

    void receive_packet(            event_handler_ns& eh,
                                    beltpp::event_item& ev_it,
                                    beltpp::ip_address address,
                                    beltpp::socket::packets& packets);

    bool check_packets(             event_handler_ns& eh,
                                    std::unordered_set<beltpp::event_item const*>& set_items);

//    bool change_packet_status(      event_handler_ns& eh,
//                                    beltpp::event_item& ev_it,
//                                    beltpp::ip_address address,
//                                    beltpp::socket::packets packets,
//                                    packet_status status);

//    void dump_network();
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

    peer_ids listen(beltpp::ip_address const& address,
                    int backlog = 100) override;

    peer_ids open(beltpp::ip_address address,
                  size_t attempts = 0) override;

    void prepare_wait() override;

    packets receive(peer_id& peer) override;

    void send(peer_id const& peer,
              beltpp::packet&& pack) override;

    void timer_action() override;

    beltpp::socket::peer_type get_peer_type(peer_id const& peer) override;
    beltpp::ip_address info(peer_id const& peer) override;
    beltpp::ip_address info_connection(peer_id const& peer) override;
    beltpp::detail::session_special_data& session_data(peer_id const& peer) override;

    std::string dump() const override;

private:
    event_handler_ns* m_eh;
};


beltpp::ip_address peer_to_address(beltpp::socket::peer_id id);
beltpp::socket::peer_id address_to_peer(beltpp::ip_address address);

}// simulator_network_impl


