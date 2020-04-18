#include "network.hpp"

namespace network_simulation_impl
{

beltpp::ip_address peer_to_address(beltpp::socket::peer_id id)
{
    size_t delimiter_index = id.find("<=>");
    std::string str_address;
    if (std::string::npos != delimiter_index)
        str_address = id.substr(delimiter_index + 3);
    else
        throw std::exception();

    beltpp::ip_address address;
    address.from_string(str_address);

    return address;
}
//  network_simulation
//

network_simulation::network_simulation()
{
}

network_simulation::~network_simulation()
{
}

void network_simulation::add_handler(event_handler_ns& eh)
{
    B_UNUSED(eh)

    //TODO
}

void network_simulation::remove_handler(event_handler_ns& eh)
{
    B_UNUSED(eh)

    //TODO
}

void network_simulation::add_socket(event_handler_ns& eh, beltpp::event_item& ev_it)
{
    B_UNUSED(eh)
    B_UNUSED(ev_it)

    //TODO
}

void network_simulation::remove_socket(event_handler_ns& eh, beltpp::event_item& ev_it)
{
    B_UNUSED(eh)
    B_UNUSED(ev_it)

    //TODO
}

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

bool network_simulation::check_packets(event_handler_ns& eh,
                                       std::unordered_set<beltpp::event_item const*>& set_items)
{
    B_UNUSED(eh)
    B_UNUSED(set_items)

    for (auto const& from_it : send_receive)
        for(auto const& to_it : from_it.second)
            if (false == to_it.second.empty())
                return true;

    return false;
}


//  event_handler_ns implementation
//

event_handler_ns::event_handler_ns(network_simulation& ns) 
    : m_ns (&ns)
{
    m_ns->add_handler(*this);
}

event_handler_ns::~event_handler_ns()
{
    m_ns->remove_handler(*this);
}

event_handler::wait_result event_handler_ns::wait(std::unordered_set<event_item const*>& set_items)
{
    set_items.clear();

    if (m_timer_helper.expired())
    {
        m_timer_helper.update();
        return event_handler_ns::timer_out;
    }

    bool on_event = m_ns->check_packets(*this, set_items);

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
    , m_ns(eh.m_ns)
{
    m_ns->add_socket(*m_eh, *this);
}

socket_ns::~socket_ns()
{
    m_ns->remove_socket(*m_eh, *this);
}

socket_ns::peer_ids socket_ns::listen(ip_address const& address, int /*backlog = 100*/)
{
    peer_ids peers;

    if (m_ns->listen_attempts.find(address.local) == m_ns->listen_attempts.end())
    {
        peer_id peer = std::to_string(++index) + "<=>" + address.local.address + ":" + std::to_string(address.local.port);

        m_ns->listen_attempts[address.local] = peer;

        peers.emplace_back(peer);
    }

    return peers;
}

socket_ns::peer_ids socket_ns::open(ip_address address, size_t attempts /*= 0*/)
{
    peer_ids peers;

    //TODO check already active connection and reject open

    auto open_it = m_ns->open_attempts.find(address.remote);
    if (open_it != m_ns->open_attempts.end())
    {
        if (open_it->second.second > 0)
            --open_it->second.second;
        else
            throw std::runtime_error("nobody listening");
    }
    else
    {
        peer_id peer = std::to_string(++index) + "<=>" + address.remote.address + ":" + std::to_string(address.remote.port);

        m_ns->open_attempts[address.remote] = { peer, attempts };

        peers.emplace_back(peer);
    }

    return peers;
}

void socket_ns::prepare_wait()
{
}

socket_ns::packets socket_ns::receive(peer_id& peer)
{
    socket_ns::packets result;

    ip_address from_address;//TODO
    ip_address to_address;//TODO

    auto open_it = m_ns->open_attempts.find(to_address.remote);
    if (open_it != m_ns->open_attempts.end())
    {
        auto listen_it = m_ns->listen_attempts.find(to_address.remote);
        if (listen_it != m_ns->listen_attempts.end())
        {
            m_ns->send_receive[from_address][to_address].emplace_back(beltpp::stream_join());
            m_ns->send_receive[to_address][from_address].emplace_back(beltpp::stream_join());

            m_ns->open_attempts.erase(open_it);
            m_ns->listen_attempts.erase(listen_it);
        }
    }

    auto from_it = m_ns->send_receive.find(from_address);
    if (from_it == m_ns->send_receive.end())
        throw std::runtime_error("receive_packet() no any connections");

    auto& to = from_it->second;
    auto to_it = to.find(to_address);
    if (to_it == to.end())
        throw std::runtime_error("receive_packet() no connection with " + peer);

    for (auto& pack : to_it->second)
        result.emplace_back(std::move(pack));

    to_it->second.clear();

    return result;
}

void socket_ns::send(peer_id const& peer, beltpp::packet&& pack)
{
    ip_address from_address;//TODO
    auto from_it = m_ns->send_receive.find(from_address);
    if (from_it == m_ns->send_receive.end())
        throw std::runtime_error("send_packet() no any connections");

    ip_address to_address = m_ns->peer_to_ip[peer];
    auto& to = from_it->second;
    auto to_it = to.find(to_address);
    if (to_it == to.end())
        throw std::runtime_error("send_packet() no connection with " + peer);

    to_it->second.emplace_back(std::move(pack));
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
