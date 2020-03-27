#pragma once

#include <belt.pp/isocket.hpp>
#include <belt.pp/ievent.hpp>

#include <chrono>
#include <unordered_set>

namespace simulator_network_impl
{
class event_handler_ex : public beltpp::event_handler
{
public:
    event_handler_ex();
    ~event_handler_ex() override;

    wait_result wait(std::unordered_set<beltpp::event_item const*>& set_items) override;
    std::unordered_set<uint64_t> waited(beltpp::event_item& ev_it) const override;

    void wake() override;
    void set_timer(std::chrono::steady_clock::duration const& period) override;

    void add(beltpp::event_item& ev_it) override;
    void remove(beltpp::event_item& ev_it) override;
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
};
}// simulator_network_impl


