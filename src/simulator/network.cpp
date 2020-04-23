#include "network.hpp"

namespace network_simulation_impl
{
//  event_handler_ns implementation
//

string network_simulation::construct_peer_id(ip_address const& socket_bundle)
{
    return std::to_string(++connection_index) + "<=>" + socket_bundle.to_string();
}

//  event_handler_ns implementation
//

event_handler_ns::event_handler_ns(network_simulation& ns) 
    : m_ns (&ns)
{
    // create an empty slot for sockets
    auto insert_result = m_ns->eh_to_sockets.insert({ this, unordered_set<beltpp::event_item*>() });

    if (false == insert_result.second)
        throw std::logic_error("event handler was already added!");
}

event_handler_ns::~event_handler_ns()
{
    m_ns->eh_to_sockets.erase(this);
}

event_handler::wait_result event_handler_ns::wait(std::unordered_set<event_item const*>& event_items)
{
    event_items.clear();

    for (auto& event_item : m_ns->eh_to_sockets[this])
        event_item->prepare_wait();

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
            peer_id& open_peer = open_it->second.first;
            ip_address& open_address = open_it->second.second;
            event_item* open_socket = m_ns->peer_to_socket[open_peer];
            
            ip_address listen_address = listen_it->second.second;
            listen_address.remote = open_address.local;
            peer_id listen_peer = m_ns->construct_peer_id(listen_address);
            event_item* listen_socket = m_ns->peer_to_socket[listen_it->second.first];

            // create symmetric connections
            m_ns->send_receive[open_socket][listen_peer].emplace_back(beltpp::stream_join());
            m_ns->send_receive[listen_socket][open_peer].emplace_back(beltpp::stream_join());

            // fill associations for future use
            m_ns->peer_to_peer.insert({ open_peer, listen_peer });
            m_ns->peer_to_peer.insert({ listen_peer, open_peer });

            m_ns->peer_to_ip.insert({ open_peer, open_address });
            m_ns->peer_to_ip.insert({ listen_peer, listen_address });

            m_ns->peer_to_socket.insert({ open_peer, open_socket });
            m_ns->peer_to_socket.insert({ listen_peer, listen_socket });

            // close open attempt
            m_ns->open_attempts.erase(open_it);
        }
    }

    //TODO connect open/open pairs

    // check sent packets to my sockets
    for (auto const& item : m_ns->send_receive)
        if (item.second.size() && m_ns->eh_to_sockets[this].count(item.first))
            event_items.insert(item.first);

    bool on_demand = false;//TODO ??
    bool on_event = (false == event_items.empty());

    if (false == on_demand && false == on_event)
        return event_handler_ns::nothing;

    if (on_demand && false == on_event)
        return event_handler_ns::on_demand;

    if (false == on_demand && on_event)
        return event_handler_ns::event;

    return event_handler_ns::on_demand_and_event;
}

std::unordered_set<uint64_t> event_handler_ns::waited(event_item& /*ev_it*/) const
{
    return std::unordered_set<uint64_t>();//TODO ??
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
    auto& sockets = m_ns->eh_to_sockets[this];
    auto insert_relult = sockets.insert(&ev_it);

    if(false == insert_relult.second)
        throw std::logic_error("socket was already added!");
}

void event_handler_ns::remove(beltpp::event_item& ev_it)
{
    auto& sockets = m_ns->eh_to_sockets[this];

    sockets.erase(&ev_it);
}


//  socket_ns implementation
//

socket_ns::socket_ns(event_handler_ns& eh)
    : socket(eh)
    , m_ns(eh.m_ns)
{
}

socket_ns::~socket_ns()
{
}

socket_ns::peer_ids socket_ns::listen(ip_address const& address, int /*backlog = 100*/)
{
    peer_ids peers;

    if (m_ns->listen_attempts.find(address.local) != m_ns->listen_attempts.end())
        throw std::logic_error("ip is already listening : " + address.to_string());

    peer_id peer = m_ns->construct_peer_id(address);

    m_ns->listen_attempts[address.local] = { peer, address };

    // probably useless this two
    m_ns->peer_to_ip[peer] = address;
    m_ns->peer_to_socket[peer] = this;

    peers.emplace_back(peer);

    return peers;
}

socket_ns::peer_ids socket_ns::open(ip_address address, size_t /*attempts = 0*/)
{
    peer_ids peers;
    
    if (address.remote.empty())
    {
        address.remote = address.local;
        address.local = ip_destination();
    }

    if (m_ns->open_attempts.find(address.remote) != m_ns->open_attempts.end())
        throw std::logic_error("ip address os already opening : " + address.to_string());

    peer_id peer = m_ns->construct_peer_id(address);

    //check already active connection and reject open
    if (m_ns->send_receive.find(this) != m_ns->send_receive.end())
    {
        auto& my_buffers = m_ns->send_receive[this];

        for (auto& item : my_buffers)
            if (address == m_ns->peer_to_ip[item.first])
                throw std::logic_error("connection is already open : " + address.to_string());

        //TODO local/remote conflict ??
    }

    m_ns->open_attempts[address.remote] = { peer, address };

    peers.emplace_back(peer);

    return peers;
}

void socket_ns::prepare_wait()
{
}

socket_ns::packets socket_ns::receive(peer_id& peer)
{
    socket_ns::packets result;

    auto my_buffers_it = m_ns->send_receive.find(this);
    if (my_buffers_it == m_ns->send_receive.end())
        throw std::runtime_error("receive_packet() no any connection");

    bool drop_received = false;
    auto& my_buffers = my_buffers_it->second;
    
    for (auto& item : my_buffers)
        if(false == item.second.empty())
        {
            peer = item.first;

            for (auto& pack : item.second)
            {
                drop_received = (pack.type() == beltpp::stream_drop::rtt);

                result.emplace_back(std::move(pack));
                
                if (drop_received)
                    break;
            }

            item.second.clear();

            break;
        }

    if (drop_received)
    {
        my_buffers.erase(peer);

        //clear storage
        m_ns->peer_to_ip.erase(peer);
        m_ns->peer_to_peer.erase(peer);
        m_ns->peer_to_socket.erase(peer);
    }

    return result;
}

void socket_ns::send(peer_id const& peer, beltpp::packet&& pack)
{
    auto receiver_socket_it = m_ns->peer_to_socket.find(peer);
    if (receiver_socket_it == m_ns->peer_to_socket.end())
        throw std::logic_error("send_packet() peer_to_socket association error");

    auto receiver_it = m_ns->send_receive.find(receiver_socket_it->second);
    if (receiver_it == m_ns->send_receive.end())
        throw std::logic_error("send_packet() no any connections");

    auto sender_peer_it = m_ns->peer_to_peer.find(peer);
    if (sender_peer_it == m_ns->peer_to_peer.end())
        throw std::logic_error("send_packet() peer_to_peer association error");
    
    auto sender_peer = sender_peer_it->second;
    auto& receiver_buffer = receiver_it->second[sender_peer];

    if (pack.type() == beltpp::stream_drop::rtt)
    {
        // remove connection from peer to me
        auto my_buffers_it = m_ns->send_receive.find(this);
        if (my_buffers_it == m_ns->send_receive.end())
            throw std::logic_error("send_packet() my all connections are droped");

        auto peer_buffer_it = my_buffers_it->second.find(peer);
        if (peer_buffer_it == my_buffers_it->second.end())
            throw std::logic_error("send_packet() connection is already droped");

        my_buffers_it->second.erase(peer_buffer_it);

        //clear storage
        m_ns->peer_to_ip.erase(sender_peer);
        m_ns->peer_to_peer.erase(sender_peer);
        m_ns->peer_to_socket.erase(sender_peer);
    }

    receiver_buffer.emplace_back(std::move(pack));
}

void socket_ns::timer_action()
{
}

socket::peer_type socket_ns::get_peer_type(peer_id const& /*peer*/)
{
    //TODO ??

    socket::peer_type type = socket::peer_type::streaming_opened;

    return type;
}

ip_address socket_ns::info(peer_id const& peer)
{
    if(m_ns->peer_to_ip.find(peer) != m_ns->peer_to_ip.end())
        return m_ns->peer_to_ip[peer];

    throw std::runtime_error("info() unknown peer " + peer);
}

ip_address socket_ns::info_connection(peer_id const& peer)
{
    // for p2p only can work as info

    return info(peer);
}

beltpp::detail::session_special_data& socket_ns::session_data(peer_id const& /*peer*/)
{
    // usless for p2p

    return temp_special_data;
}

std::string socket_ns::dump() const
{
    return std::string("dump is not implemented yet!");
}

}// network_simulation_impl
