#include "network.hpp"

#include <exception>
#include <string>

namespace simulator_network_impl
{
using beltpp::event_handler;
using beltpp::event_item;
using beltpp::socket;
using beltpp::ip_address;
using beltpp::packet;

using std::unordered_set;
using std::string;

//  network_simulation
//
network_simulation::network_simulation()
{
}
network_simulation::~network_simulation()
{
}
//  event_handler_ex implementation
//
event_handler_ex::event_handler_ex(network_simulation& ns)
{
}
event_handler_ex::~event_handler_ex()
{
}
event_handler::wait_result event_handler_ex::wait(unordered_set<event_item const*>& set_items)
{
    throw std::logic_error("event_handler_ex::wait");
}
unordered_set<uint64_t> event_handler_ex::waited(event_item& ev_it) const
{
    throw std::logic_error("event_handler_ex::waited");
}
void event_handler_ex::wake()
{
    throw std::logic_error("event_handler_ex::wake");
}
void event_handler_ex::set_timer(std::chrono::steady_clock::duration const& period)
{
    throw std::logic_error("event_handler_ex::set_timer");
}
void event_handler_ex::add(event_item& ev_it)
{
    throw std::logic_error("event_handler_ex::add");
}
void event_handler_ex::remove(beltpp::event_item& ev_it)
{
    throw std::logic_error("event_handler_ex::remove");
}


//  socket_ex   implementation
//
socket_ex::socket_ex(event_handler_ex& eh)
    : socket(eh)
{
}
socket_ex::~socket_ex()
{
}
socket_ex::peer_ids socket_ex::listen(ip_address const& address,
                                      int backlog/* = 100*/)
{
    throw std::logic_error("socket_ex::listen");
}
socket_ex::peer_ids socket_ex::open(ip_address address,
                                    size_t attempts/* = 0*/)
{
    throw std::logic_error("socket_ex::open");
}
void socket_ex::prepare_wait()
{
    throw std::logic_error("socket_ex::prepare_wait");
}

socket_ex::packets socket_ex::receive(peer_id& peer)
{
    throw std::logic_error("socket_ex::receive");
}

void socket_ex::send(peer_id const& peer,
                     beltpp::packet&& pack)
{
    throw std::logic_error("socket_ex::send");
}
void socket_ex::timer_action()
{
    throw std::logic_error("socket_ex::timer_action");
}
socket::peer_type socket_ex::get_peer_type(peer_id const& peer)
{
    throw std::logic_error("socket_ex::get_peer_type");
}
ip_address socket_ex::info(peer_id const& peer)
{
    throw std::logic_error("socket_ex::info");
}
ip_address socket_ex::info_connection(peer_id const& peer)
{
    throw std::logic_error("socket_ex::info_connection");
}
beltpp::detail::session_special_data& socket_ex::session_data(peer_id const& peer)
{
    throw std::logic_error("socket_ex::session_data");
}
string socket_ex::dump() const
{
    throw std::logic_error("socket_ex::dump");
}

}// simulator_network_impl
