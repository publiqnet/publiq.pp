#include "network.hpp"

namespace network_simulation_impl
{

//  network_simulation
//

network_simulation::network_simulation()
{
}

network_simulation::~network_simulation()
{
}

void network_simulation::add_handler(event_handler_ns& /*eh*/)
{
    //TODO
}

void network_simulation::remove_handler(event_handler_ns& /*eh*/)
{
    //TODO
}

//void network_simulation::add_socket(event_handler_ns& eh, beltpp::event_item& ev_it)
//{
//    sockets socks;
//    socks[ev_it] = connections();
//    network_status[eh] = socks;
//}
//
//void network_simulation::add_connection(event_handler_ns& eh,
//                                        beltpp::event_item& ev_it,
//                                        beltpp::ip_address address)
//{
//    std::map< connection, packs > connect;
//    connect[std::make_pair(address, connection_status::connection_listen)] = packs();
//    network_status[eh][ev_it].push_back(connect);
//}
//
//bool network_simulation::change_connection_status(event_handler_ns& eh,
//                                                  beltpp::event_item& ev_it,
//                                                  beltpp::ip_address address,
//                                                  connection_status status)
//{
//    B_UNUSED(address);
//    B_UNUSED(status);
//
//    for (auto& connections : network_status[eh][ev_it])
//    {
//        auto it = connections.begin();
//        while (it != connections.end())
//        {
//            //if (it->first.first == address)
//            //{
//            //    auto value = std::move(it->second);
//            //    connections.erase(it);
//            //    connections[std::make_pair(address, status)] = value;
//            //    return true;
//            //}
//        }
//    }
//    return false;
//}

void network_simulation::send_packet(event_handler_ns& eh,
                                     //beltpp::event_item& ev_it,
                                     ip_address const& to_address,
                                     packet const& packet)
{
    B_UNUSED(eh);
    ip_address from_address;//TODO find it, use eh

    auto from_it = send_receive_status.find(from_address);
    if (from_it == send_receive_status.end())
        throw std::exception();//TODO

    auto& to = from_it->second;
    auto to_it = to.find(to_address);
    if(to_it == to.end())
        throw std::exception();//TODO

    B_UNUSED(packet);
    //to_it->second.emplace_back(packet);
}

void network_simulation::receive_packet(event_handler_ns& eh,
                                        //beltpp::event_item& ev_it,
                                        beltpp::ip_address const& from_address,
                                        beltpp::socket::packets& packets)
{
    packets.clear();

    B_UNUSED(eh);
    ip_address to_address;//TODO find it, use eh

    auto from_it = send_receive_status.find(from_address);
    if (from_it == send_receive_status.end())
        throw std::exception();//TODO

    auto& to = from_it->second;
    auto to_it = to.find(to_address);
    if (to_it == to.end())
        throw std::exception();//TODO

    B_UNUSED(packets);
    //packets.emplace_back(to_it->second);
    to_it->second.clear();
}

//bool network_simulation::check_packets(event_handler_ns& eh,
//                                       std::unordered_set<beltpp::event_item const*>& set_items)
//{
//    bool found = false;
//
//    auto it = network_status[eh].begin();
//    while (it != network_status[eh].end())
//    {
//        found = false;
//        
//        for (auto& connection : it->second)
//        {
//            for (auto& conn : connection)
//            {
//                if (conn.first.second == connection_status::connection_open)
//                {
//                    for (auto& pack : conn.second)
//                    {
//                        if (pack.second == packet_status::sent)
//                            found = true;
//                    }
//                }
//            }
//        }
//
//        if (found)
//        {
//            //beltpp::event_item const* item = it->first;
//            //set_items.insert(item);
//        }
//    }
//    return found;
//}


//  event_handler_ns implementation
//

event_handler_ns::event_handler_ns(network_simulation& ns) 
    : m_ns (&ns)
{
}

event_handler_ns::~event_handler_ns()
{
}

event_handler::wait_result event_handler_ns::wait(std::unordered_set<event_item const*>& set_items)
{
    set_items.clear();

    if (m_timer_helper.expired())
    {
        m_timer_helper.update();
        return event_handler_ns::timer_out;
    }

    bool on_event = false;// m_ns->check_packets(*this, set_items);

    bool on_timer = m_timer_helper.expired();

    if (on_timer)
        m_timer_helper.update();

    if (false == on_timer &&
        false == on_event)
        return event_handler_ns::nothing;

    if (on_timer &&
        false == on_event)
        return event_handler_ns::timer_out;

    if (false == on_timer &&
        on_event)
        return event_handler_ns::event;

    return event_handler_ns::timer_out_and_event;
}

std::unordered_set<uint64_t> event_handler_ns::waited(event_item& /*ev_it*/) const
{
    std::unordered_set<uint64_t> set;

    //TODO

    return set;
}

void event_handler_ns::wake()
{
}

void event_handler_ns::set_timer(std::chrono::steady_clock::duration const& period)
{
    m_timer_helper.set(period);
}

void event_handler_ns::add(event_item& /*ev_it*/)
{
//    m_ns->add_socket(*this, ev_it);
}

void event_handler_ns::remove(beltpp::event_item& /*ev_it*/)
{
//    m_ns->remove_socket(*this, ev_it); //not sure
}


//  socket_ns implementation
//

socket_ns::socket_ns(event_handler_ns& eh)
    : socket(eh)
    , m_eh(&eh)
{
}

socket_ns::~socket_ns()
{
}

socket_ns::peer_ids socket_ns::listen(ip_address const& /*address*/, int /*backlog = 100*/)
{
    peer_ids peers;



    return peers;
}

socket_ns::peer_ids socket_ns::open(ip_address /*address*/, size_t /*attempts = 0*/)
{
    peer_ids peers;

    //if (m_eh->m_ns->change_connection_status(*m_eh,
    //                                         *this,
    //                                         address,
    //                                         status))
    //    peers.emplace_back(address_to_peer(address));

    return peers;
}

void socket_ns::prepare_wait()
{
}

socket_ns::packets socket_ns::receive(peer_id& peer)
{
    socket_ns::packets result;
    
    peer = peer_id();

    m_eh->m_ns->receive_packet(*m_eh,
                               //*this,
                               ip_address(),
                               result);

    return result;
}

void socket_ns::send(peer_id const& peer, beltpp::packet&& pack)
{
    B_UNUSED(peer);
    m_eh->m_ns->send_packet(*m_eh,
                            //*this,
                            ip_address(),// peer_to_address(peer),
                            pack);
}

void socket_ns::timer_action()
{
}

socket::peer_type socket_ns::get_peer_type(peer_id const& /*peer*/)
{
    socket::peer_type type = socket::peer_type::listening;

    //TODO

    return type;
}

ip_address socket_ns::info(peer_id const& /*peer*/)
{
    ip_address addr;

    //TODO

    return addr;
}

ip_address socket_ns::info_connection(peer_id const& /*peer*/)
{
    ip_address addr;

    //TODO

    return addr;
}

beltpp::detail::session_special_data& socket_ns::session_data(peer_id const& /*peer*/)
{
    //TODO

    return temp_special_data;
}

std::string socket_ns::dump() const
{
    return std::string("dump is not implemented yet!");
}

}// network_simulation_impl
