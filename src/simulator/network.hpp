#pragma once

#include <belt.pp/isocket.hpp>
#include <belt.pp/ievent.hpp>
#include <belt.pp/queue.hpp>
#include <belt.pp/timer.hpp>

#include <chrono>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <vector>

namespace simulator_network_impl
{

class event_slot
{
public:
    event_slot(uint64_t item_id = 0, beltpp::event_item* pitem = nullptr)
        : m_item_id(item_id)
        , m_pitem(pitem)
    {}

    bool m_closed = false;
    uint64_t m_item_id = 0;
    beltpp::event_item* m_pitem = nullptr;
};

using event_slots = beltpp::queue<event_slot>;

class network_simulation
{
public:
    enum connection_status {listen, open, close};

    network_simulation();
    ~network_simulation();
private:
    std::map<beltpp::ip_address, connection_status> status; //?
    std::map<beltpp::socket::peer_id, beltpp::packet> messages; //?
};

class event_handler_ex : public beltpp::event_handler
{
public:
    event_handler_ex(network_simulation& ns);
    ~event_handler_ex() override;

    wait_result wait(std::unordered_set<beltpp::event_item const*>& set_items) override;
    std::unordered_set<uint64_t> waited(beltpp::event_item& ev_it) const override;

    void wake() override;
    void set_timer(std::chrono::steady_clock::duration const& period) override;

    void add(beltpp::event_item& ev_it) override;
    void remove(beltpp::event_item& ev_it) override;

    beltpp::timer m_timer_helper;
    std::list<event_slots> m_ids;
    std::unordered_map<beltpp::event_item*, std::unordered_set<uint64_t>> m_event_item_ids;
    std::unordered_set<beltpp::event_item*> m_event_items;
    std::unordered_set<uint64_t> sync_eh_ids;
    network_simulation m_ns;
};

class socket_ex : public beltpp::socket
{
public:
    using peer_id = socket::peer_id;
    using peer_ids = socket::peer_ids;
    using packets = socket::packets;

    socket_ex(event_handler_ex& eh);
    ~socket_ex() override;

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
    event_handler_ex* m_peh;
};
}// simulator_network_impl


