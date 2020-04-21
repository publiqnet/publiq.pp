#include "network.hpp"

namespace network_simulation_impl
{

//beltpp::ip_address peer_to_address(beltpp::socket::peer_id id)
//{
//    size_t delimiter_index = id.find("<=>");
//    std::string str_address;
//    if (std::string::npos != delimiter_index)
//        str_address = id.substr(delimiter_index + 3);
//    else
//        throw std::exception();
//
//    beltpp::ip_address address;
//    address.from_string(str_address);
//
//    return address;
//}

string construct_peer_id(uint64_t id, ip_address const& socket_bundle)
{
    return std::to_string(id) + "<=>" + socket_bundle.to_string();
}

//  event_handler_ns implementation
//

event_handler_ns::event_handler_ns(network_simulation& ns) 
    : m_ns (&ns)
{
    auto insert_result = m_ns->handler_to_sockets.insert({ this, unordered_set<beltpp::event_item*>() });

    if (false == insert_result.second)
        throw std::runtime_error("event handler was already added!");
}

event_handler_ns::~event_handler_ns()
{
    m_ns->handler_to_sockets.erase(this);
}

event_handler::wait_result event_handler_ns::wait(std::unordered_set<event_item const*>& event_items)
{
    event_items.clear();

    for (auto& event_item : m_ns->handler_to_sockets[this])
    {
        event_item->prepare_wait();
    }

    if (m_timer_helper.expired())
    {
        m_timer_helper.update();
        return event_handler_ns::timer_out;
    }

    // connect all waiting listen/open pairs
    auto open_it = m_ns->open_attempts.begin();
    for (; open_it != m_ns->open_attempts.end(); ++open_it)
    {
        auto listen_it = m_ns->listen_attempts.find(open_it->first);

        if (listen_it != m_ns->listen_attempts.end())
        {
            ip_address& open_address = open_it->second.first.second;
            ip_address& listen_address = listen_it->second.second;

            m_ns->send_receive[open_address][listen_address].emplace_back(beltpp::stream_join());
            m_ns->send_receive[listen_address][open_address].emplace_back(beltpp::stream_join());

            m_ns->peer_to_ip.insert({ listen_it->second.first,{ listen_address, open_address } });
            m_ns->peer_to_ip.insert({ open_it->second.first.first, { open_address, listen_address } });

            m_ns->open_attempts.erase(open_it);
            m_ns->listen_attempts.erase(listen_it);
        }
    }

    for (auto const& from_it : m_ns->send_receive)
        for (auto const& to_it : from_it.second)
            if (false == to_it.second.empty() && this == m_ns->ip_to_eh[to_it.first])
            {
                beltpp::event_item const* socket;

                //TODO

                event_items.insert(socket);
            }

    bool on_timer = m_timer_helper.expired(); // @Tigran why 2 times ?

    if (on_timer)
        m_timer_helper.update();

    // bellow is a copy from event_handler_ex.wait()

    bool on_event = (false == event_items.empty());

    if (false == on_demand &&
        false == on_timer &&
        false == on_event)
        return event_handler_ns::nothing;

    if (on_demand &&
        false == on_timer &&
        false == on_event)
        return event_handler_ns::on_demand;

    if (false == on_demand &&
        on_timer &&
        false == on_event)
        return event_handler_ns::timer_out;

    if (false == on_demand &&
        false == on_timer &&
        on_event)
        return event_handler_ns::event;

    if (on_demand &&
        on_timer &&
        false == on_event)
        return event_handler_ns::on_demand_and_timer_out;

    if (on_demand &&
        false == on_timer &&
        on_event)
        return event_handler_ns::on_demand_and_event;

    if (false == on_demand &&
        on_timer &&
        on_event)
        return event_handler_ns::timer_out_and_event;

    return event_handler_ns::on_demand_and_timer_out_and_event;
}

std::unordered_set<uint64_t> event_handler_ns::waited(event_item& /*ev_it*/) const
{
    return std::unordered_set<uint64_t>();
}

void event_handler_ns::wake()
{
}

void event_handler_ns::set_timer(std::chrono::steady_clock::duration const& period)
{
    m_timer_helper.set(period);
}

void event_handler_ns::add(event_item& ev_it)
{
    auto& sockets = m_ns->handler_to_sockets[this];
    auto insert_relult = sockets.insert(&ev_it);

    if(false == insert_relult.second)
        throw std::runtime_error("socket was already added!");
}

void event_handler_ns::remove(beltpp::event_item& ev_it)
{
    auto& sockets = m_ns->handler_to_sockets[this];

    sockets.erase(&ev_it);
}


//  socket_ns implementation
//

socket_ns::socket_ns(event_handler_ns& eh)
    : socket(eh)
    , m_eh(&eh)
    , m_ns(eh.m_ns)
{
    m_eh->add(*this);
}

socket_ns::~socket_ns()
{
    m_eh->remove(*this);
}

socket_ns::peer_ids socket_ns::listen(ip_address const& address, int /*backlog = 100*/)
{
    peer_ids peers;

    if (m_ns->listen_attempts.find(address.local) == m_ns->listen_attempts.end())
    {
        peer_id peer = construct_peer_id(++m_ns->connection_index, address);

        m_ns->ip_to_eh[address] = m_eh;

        m_ns->listen_attempts[address.local] = { peer, address };

        peers.emplace_back(peer);
    }

    return peers;
}

socket_ns::peer_ids socket_ns::open(ip_address address, size_t attempts /*= 0*/)
{
    peer_ids peers;
    
    if (address.remote.empty())
    {
        address.remote = address.local;
        address.local = ip_destination();
    }

    //check already active connection and reject open
    if (m_ns->send_receive.find(address) != m_ns->send_receive.end())
        throw std::runtime_error("connection is already open : " + address.to_string());

    auto open_it = m_ns->open_attempts.find(address.remote);
    if (open_it != m_ns->open_attempts.end())
    {
        ++open_it->second.second;
    }
    else
    {
        peer_id peer = construct_peer_id(++m_ns->connection_index, address);

        m_ns->ip_to_eh[address] = m_eh;

        m_ns->open_attempts[address.remote] = { { peer, address }, ++attempts };

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

    auto& peers = m_ns->peer_to_ip[peer];
    ip_address from_address = peers.first;
    ip_address to_address = peers.second;

    auto from_it = m_ns->send_receive.find(from_address);
    if (from_it == m_ns->send_receive.end())
        throw std::runtime_error("receive_packet() no any connections");

    auto& to = from_it->second;
    auto to_it = to.find(to_address);
    if (to_it == to.end())
        throw std::runtime_error("receive_packet() no connection with " + peer);

    //TODO what if read value is drop?

    for (auto& pack : to_it->second)
        result.emplace_back(std::move(pack));

    to_it->second.clear();

    return result;
}

void socket_ns::send(peer_id const& peer, beltpp::packet&& pack)
{
    auto& peers = m_ns->peer_to_ip[peer];
    ip_address from_address = peers.first;
    ip_address to_address = peers.second;

    if (pack.type() == beltpp::stream_drop::rtt)
    {
        // remove connection from peer to me
        auto delete_from_it = m_ns->send_receive.find(to_address);
        if (delete_from_it != m_ns->send_receive.end())
        {
            auto& delete_to = delete_from_it->second;

            auto delete_to_it = delete_to.find(from_address);
            if (delete_to_it != delete_to.end())
                delete_to.erase(delete_to_it);
        }

        auto& ip = m_ns->peer_to_ip[peer].first;

        m_ns->ip_to_eh.erase(ip);
        m_ns->peer_to_ip.erase(peer);
    }

    auto from_it = m_ns->send_receive.find(from_address);
    if (from_it == m_ns->send_receive.end())
        throw std::runtime_error("send_packet() no any connections");

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
    //TODO

    socket::peer_type type = socket::peer_type::streaming_opened;

    return type;
}

ip_address socket_ns::info(peer_id const& peer)
{
    if(m_ns->peer_to_ip.find(peer) != m_ns->peer_to_ip.end())
        return m_ns->peer_to_ip[peer].second;

    throw std::runtime_error("info() unknown peer " + peer);
}

ip_address socket_ns::info_connection(peer_id const& peer)
{
    //TODO

    if (m_ns->peer_to_ip.find(peer) != m_ns->peer_to_ip.end())
        return m_ns->peer_to_ip[peer].second;

    throw std::runtime_error("info() unknown peer " + peer);
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
