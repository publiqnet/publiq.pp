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
event_handler_ex::event_handler_ex(network_simulation& ns) : m_ns(ns)
{
}
event_handler_ex::~event_handler_ex()
{
}
event_handler::wait_result event_handler_ex::wait(unordered_set<event_item const*>& set_items)
{
    set_items.clear();
    m_event_item_ids.clear();

    for (auto& event_item_in_set : m_event_items)
    {
        event_item_in_set->prepare_wait();
    }

    if (m_timer_helper.expired())
    {
        m_timer_helper.update();
        return event_handler_ex::timer_out;
    }

    std::unordered_set<uint64_t> set_ids;
    auto it = sync_eh_ids.begin();
    if (it != sync_eh_ids.end())
    {
        set_ids.insert(*it);
        sync_eh_ids.erase(it);
    }

    bool on_demand = false;
    if (set_ids.empty())
        //set_ids = m_ns.wait(m_timer_helper, on_demand); get ids from network simulator

    auto it = set_ids.begin();
    while (it != set_ids.end())
    {
        auto id = *it;
        bool found = false;
        for (auto const& ids_item : m_ids)
        {
            if (ids_item.valid_index(id))
            {
                auto& ref_item = ids_item[id];
                event_item* pitem = ref_item.m_pitem;
                set_items.insert(pitem);
                found = true;

                auto& item = m_event_item_ids[pitem];
                item.insert(ref_item.m_item_id);

                break;
            }
        }

        if (false == found)
            it = set_ids.erase(it);
        else
            ++it;
    }

    bool on_timer = m_timer_helper.expired();
    bool on_event = (false == set_ids.empty());

    if (on_timer)
        m_timer_helper.update();

    if (false == on_demand &&
        false == on_timer &&
        false == on_event)
        return event_handler_ex::nothing;

    if (on_demand &&
        false == on_timer &&
        false == on_event)
        return event_handler_ex::on_demand;
    if (false == on_demand &&
        on_timer &&
        false == on_event)
        return event_handler_ex::timer_out;
    if (false == on_demand &&
        false == on_timer &&
        on_event)
        return event_handler_ex::event;

    if (on_demand &&
        on_timer &&
        false == on_event)
        return event_handler_ex::on_demand_and_timer_out;
    if (on_demand &&
        false == on_timer &&
        on_event)
        return event_handler_ex::on_demand_and_event;
    if (false == on_demand &&
        on_timer &&
        on_event)
        return event_handler_ex::timer_out_and_event;

    /*if (on_demand &&
        on_timer &&
        on_event)*/
    return event_handler_ex::on_demand_and_timer_out_and_event;
}
unordered_set<uint64_t> event_handler_ex::waited(event_item& ev_it) const
{
    return m_event_item_ids.at(&ev_it);
}
void event_handler_ex::wake()
{
    //
}
void event_handler_ex::set_timer(std::chrono::steady_clock::duration const& period)
{
    m_timer_helper.set(period);
}
void event_handler_ex::add(event_item& ev_it)
{
    m_event_items.insert(&ev_it);
}
void event_handler_ex::remove(beltpp::event_item& ev_it)
{
    m_event_items.erase(&ev_it);
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
