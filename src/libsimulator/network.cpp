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
    , m_wake_triggered(false)
{
    // create an empty slot for sockets
    auto insert_result = m_ns->eh_to_sockets.insert({ this, unordered_set<string>() });

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

    for (auto& socket_name : m_ns->eh_to_sockets[this])
        if (nullptr != m_ns->name_to_sockets[socket_name].second)
            m_ns->name_to_sockets[socket_name].second->prepare_wait();

    if (m_timer_helper.expired())
    {
        m_timer_helper.update();
        return event_handler_ns::timer_out;
    }

    // connect all waiting listen/open pairs
    auto open_it = m_ns->open_attempts.begin();
    for (; open_it != m_ns->open_attempts.end();)
    {
        auto listen_it = m_ns->listen_attempts.find(open_it->first);

        if (listen_it == m_ns->listen_attempts.end())
            ++open_it;
        else
        {
            peer_id& open_peer = open_it->second.first;
            ip_address& open_address = open_it->second.second;
            string& open_socket_name = m_ns->peer_to_socket[open_peer];
            
            ip_address listen_address = listen_it->second.second;
            listen_address.remote = open_address.local;
            peer_id listen_peer = m_ns->construct_peer_id(listen_address);
            string& listen_socket_name = m_ns->peer_to_socket[listen_it->second.first];

            // create symmetric connections
            m_ns->send_receive[open_socket_name][listen_peer].emplace_back(beltpp::stream_join());
            m_ns->send_receive[listen_socket_name][open_peer].emplace_back(beltpp::stream_join());

            // fill associations for future use
            m_ns->peer_to_peer.insert({ open_peer, listen_peer });
            m_ns->peer_to_peer.insert({ listen_peer, open_peer });

            m_ns->peer_to_ip.insert({ open_peer, open_address });
            m_ns->peer_to_ip.insert({ listen_peer, listen_address });

            m_ns->peer_to_socket.insert({ listen_peer, listen_socket_name });

            // close open attempt
            open_it = m_ns->open_attempts.erase(open_it);
        }
    }

    // connect all waiting open/open pairs
    auto first_it = m_ns->open_attempts.begin();
    for (; first_it != m_ns->open_attempts.end();)
    {
        bool pair_found = false;
        auto second_it = first_it;
        for (++second_it; second_it != m_ns->open_attempts.end(); ++second_it)
        {
            if (first_it->second.second.local != second_it->second.second.remote ||
                first_it->second.second.remote != second_it->second.second.local)
                ++second_it;
            else
            {
                pair_found = true;

                peer_id& first_peer = first_it->second.first;
                ip_address& first_address = first_it->second.second;
                string& first_socket_name = m_ns->peer_to_socket[first_peer];

                peer_id& second_peer = second_it->second.first;
                ip_address& second_address = second_it->second.second;
                string& second_socket_name = m_ns->peer_to_socket[second_peer];

                // create symmetric connections
                m_ns->send_receive[first_socket_name][second_peer].emplace_back(beltpp::stream_join());
                m_ns->send_receive[second_socket_name][first_peer].emplace_back(beltpp::stream_join());

                // fill associations for future use
                m_ns->peer_to_peer.insert({ first_peer, second_peer });
                m_ns->peer_to_peer.insert({ second_peer, first_peer });

                m_ns->peer_to_ip.insert({ first_peer, first_address });
                m_ns->peer_to_ip.insert({ second_peer, second_address });

                // close open attempt
                second_it = m_ns->open_attempts.erase(second_it);
                first_it = m_ns->open_attempts.erase(first_it);
            }

            if (pair_found)
                break;
            else
                ++first_it;
        }
    }

    // check sent packets to my sockets
    auto& sockets = m_ns->eh_to_sockets[this];
    for (auto const& item : m_ns->send_receive)
        if (false == item.second.empty() && sockets.count(item.first))
        {
            auto& temp = m_ns->name_to_sockets[item.first];

            if (nullptr == temp.second)
                event_items.insert(temp.first);
            else
                event_items.insert(temp.first);
        }

    bool on_demand = m_wake_triggered;
    m_wake_triggered = false;
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
    return std::unordered_set<uint64_t>();
}

void event_handler_ns::wake()
{
    m_wake_triggered = true;
}

void event_handler_ns::set_timer(std::chrono::steady_clock::duration const& period)
{
    m_timer_helper.set(period);
}

void event_handler_ns::add(event_item& ev_it)
{
    // chemistry implementation
    if (m_ns->socket_to_name.find(&ev_it) == m_ns->socket_to_name.end())
    {
        m_ns->socket_to_name[&ev_it] = last_socket_name;
        m_ns->name_to_sockets[last_socket_name].second = &ev_it;
    }

    auto& sockets = m_ns->eh_to_sockets[this];
    auto insert_relult = sockets.insert(m_ns->socket_to_name[&ev_it]);

    if(false == insert_relult.second)
        throw std::logic_error("socket was already added!");
}

void event_handler_ns::remove(beltpp::event_item& ev_it)
{
    auto& sockets = m_ns->eh_to_sockets[this];

    sockets.erase(m_ns->socket_to_name[&ev_it]);
    m_ns->send_receive.erase(m_ns->socket_to_name[&ev_it]);

    m_ns->socket_to_name.erase(&ev_it);
}


//  socket_ns implementation
//

socket_ns::socket_ns(event_handler_ns& eh, string& address, string name)
    : socket(eh)
    , m_name(name)
    , m_address(address)
    , m_eh(&eh)
    , m_ns(eh.m_ns)
{
    if (false == m_ns->socket_to_name.insert({ this, m_name }).second)
        throw std::logic_error("socket name is not unique!");

    if (false == m_ns->name_to_sockets.insert({ m_name, { this, nullptr } }).second)
        throw std::logic_error("socket name is not unique!");

    // chemistry support
    m_eh->last_socket_name = m_name;
}

socket_ns::~socket_ns()
{
    m_ns->socket_to_name.erase(this);
    m_ns->name_to_sockets.erase(m_name);
}

socket_ns::peer_ids socket_ns::listen(ip_address const& address, int /*backlog = 100*/)
{
    peer_ids peers;
    ip_address tmp_address = address;
    tmp_address.local.address = m_address;

    if (m_ns->listen_attempts.find(tmp_address.local) != m_ns->listen_attempts.end())
        throw std::runtime_error("ip is already listening : " + tmp_address.to_string());

    peer_id peer = m_ns->construct_peer_id(tmp_address);

    m_ns->listen_attempts[tmp_address.local] = { peer, tmp_address };
    m_ns->peer_to_socket[peer] = m_name;
    m_ns->peer_to_ip[peer] = tmp_address;

    peers.emplace_back(peer);

    return peers;
}

socket_ns::peer_ids socket_ns::open(ip_address address, size_t /*attempts = 0*/)
{
    peer_ids peers;
    address.local.address = m_address;

    if (address.remote.empty())
    {
        address.remote = address.local;
        address.local = ip_destination();
    }

    if (m_ns->open_attempts.find(address.remote) != m_ns->open_attempts.end() &&
        address == m_ns->open_attempts[address.remote].second)
        throw std::runtime_error("ip address is already opening : " + address.to_string());

    peer_id peer = m_ns->construct_peer_id(address);

    //check already active connection and reject open
    if (m_ns->send_receive.find(m_name) != m_ns->send_receive.end())
    {
        auto& my_buffers = m_ns->send_receive[m_name];

        for (auto& item : my_buffers)
            if (address == m_ns->peer_to_ip[item.first])
                throw std::runtime_error("connection is already open : " + address.to_string());
    }

    m_ns->open_attempts[address.remote] = { peer, address };

    m_ns->peer_to_socket[peer] = m_name;

    peers.emplace_back(peer);

    return peers;
}

void socket_ns::prepare_wait()
{
}

socket_ns::packets socket_ns::receive(peer_id& peer)
{
    socket_ns::packets result;

    auto my_buffers_it = m_ns->send_receive.find(m_name);
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

        if (my_buffers_it->second.empty())
            m_ns->send_receive.erase(my_buffers_it);

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

    auto& receiver_buffer = receiver_it->second[sender_peer_it->second];

    if (pack.type() == beltpp::stream_drop::rtt)
    {
        // remove connection from peer to me
        auto my_buffers_it = m_ns->send_receive.find(m_name);
        if (my_buffers_it == m_ns->send_receive.end())
            throw std::logic_error("send_packet() my all connections are droped");

        auto peer_buffer_it = my_buffers_it->second.find(peer);
        if (peer_buffer_it == my_buffers_it->second.end())
            throw std::logic_error("send_packet() connection is already droped");

        my_buffers_it->second.erase(peer_buffer_it);

        if (my_buffers_it->second.empty())
            m_ns->send_receive.erase(my_buffers_it);

        //clear storage
        m_ns->peer_to_ip.erase(peer);
        m_ns->peer_to_peer.erase(peer);
        m_ns->peer_to_socket.erase(peer);
    }

    receiver_buffer.emplace_back(std::move(pack));
}

void socket_ns::timer_action()
{
}

socket::peer_type socket_ns::get_peer_type(peer_id const& /*peer*/)
{
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
