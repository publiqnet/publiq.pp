#include "network.hpp"

namespace simulator_network_impl
{

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

event_handler_ex::event_handler_ex(network_simulation& ns) : m_ns (&ns)
{

}

event_handler_ex::~event_handler_ex()
{

}

event_handler::wait_result event_handler_ex::wait(std::unordered_set<event_item const*>& set_items)
{
    set_items.clear();

    if (m_timer_helper.expired())
    {
        m_timer_helper.update();
        return event_handler_ex::timer_out;
    }

    bool on_event = m_ns->check_packets(*this, set_items);

    bool on_timer = m_timer_helper.expired();

    if (on_timer)
        m_timer_helper.update();

    if (false == on_timer &&
        false == on_event)
        return event_handler_ex::nothing;

    if (on_timer &&
        false == on_event)
        return event_handler_ex::timer_out;

    if (false == on_timer &&
        on_event)
        return event_handler_ex::event;

//    if (on_timer &&
//        on_event)
        return event_handler_ex::timer_out_and_event;
}

unordered_set<uint64_t> event_handler_ex::waited(event_item& ev_it) const
{

}

void event_handler_ex::wake()
{

}

void event_handler_ex::set_timer(std::chrono::steady_clock::duration const& period)
{
    m_timer_helper.set(period);
}

void event_handler_ex::add(event_item& ev_it)
{
    m_ns->add_socket(*this, ev_it);
}

void event_handler_ex::remove(beltpp::event_item& ev_it)
{
    //m_ns.remove_socket(*this, ev_it); not sure
}

//  socket_ex implementation
//

socket_ex::socket_ex(event_handler_ex& eh)
    : socket(eh)
{
    m_eh = &eh;
}

socket_ex::~socket_ex()
{

}

socket_ex::peer_ids socket_ex::listen(ip_address const& address,
                                      int backlog/* = 100*/)
{
    peer_ids peers;

    if (m_eh->m_ns->add_connection(*m_eh,
                                   *this,
                                   address))
        peers.emplace_back(address_to_peer(address));

    return peers;
}

socket_ex::peer_ids socket_ex::open(ip_address address,
                                    size_t attempts/* = 0*/)
{
    network_simulation::connection_status status = network_simulation::connection_status::connection_open;
    peer_ids peers;

    if (m_eh->m_ns->change_connection_status(*m_eh,
                                             *this,
                                             address,
                                             status))
        peers.emplace_back(address_to_peer(address));

    return peers;
}

void socket_ex::prepare_wait()
{

}

socket_ex::packets socket_ex::receive(peer_id& peer)
{
    socket_ex::packets result;
    m_eh->m_ns->receive_packet(*m_eh,
                               *this,
                               peer_to_address(peer),
                               result);
    return result;
}

void socket_ex::send(peer_id const& peer,
                     beltpp::packet&& pack)
{
     m_eh->m_ns->send_packet(*m_eh,
                             *this,
                             peer_to_address(peer),
                             pack);
}

void socket_ex::timer_action()
{

}

socket::peer_type socket_ex::get_peer_type(peer_id const& peer)
{

}

ip_address socket_ex::info(peer_id const& peer)
{

}

ip_address socket_ex::info_connection(peer_id const& peer)
{

}

beltpp::detail::session_special_data& socket_ex::session_data(peer_id const& peer)
{

}

string socket_ex::dump() const
{

}

}// simulator_network_impl
