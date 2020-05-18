#include "network.hpp"

namespace network_simulation_impl
{
//  event_handler_ns implementation

string network_simulation::construct_peer_id(ip_address const& socket_bundle)
{
    return std::to_string(++connection_index) + "<=>" + socket_bundle.to_string();
}

void network_simulation::process_attempts()
{
    //clear storage
    set<peer_id> to_delete;
    for (auto& peer : peers_to_drop)
    {
        bool peers_not_used = true;
        auto pair_peer = peer_to_peer[peer];

        for (auto it = receive_send.begin(); peers_not_used && it != receive_send.end(); ++it)
            peers_not_used = it->second.find(peer) == it->second.end() &&
                             it->second.find(pair_peer) == it->second.end();

        if (peers_not_used)
        {
            to_delete.insert(peer);
            to_delete.insert(pair_peer);

            peer_to_ip.erase(peer);
            peer_to_ip.erase(pair_peer);

            peer_to_peer.erase(peer);
            peer_to_peer.erase(pair_peer);

            peer_to_socket.erase(peer);
            peer_to_socket.erase(pair_peer);
        }
    }

    for (auto& peer : to_delete)
        peers_to_drop.erase(peer);

    if (to_delete.size())
        return;

    // connect all waiting open/open pairs
    auto first_it = open_attempts.begin();
    for (; first_it != open_attempts.end();)
    {
        bool pair_found = false;
        auto second_it = first_it;
        for (++second_it; second_it != open_attempts.end(); ++second_it)
        {
            if (first_it->first.local == second_it->first.remote &&
                first_it->first.remote == second_it->first.local)
            {
                pair_found = true;

                peer_id& first_peer = first_it->second;
                ip_address first_address = first_it->first;
                string& first_socket_name = peer_to_socket[first_peer];

                peer_id& second_peer = second_it->second;
                ip_address second_address = second_it->first;
                string& second_socket_name = peer_to_socket[second_peer];

                bool connection_allowed = true;
                auto refused_it = permanent_refused_connections.find(first_socket_name);
                
                if (refused_it != permanent_refused_connections.end() && 
                    refused_it->second.count(second_socket_name))
                    connection_allowed = false;
                else
                {
                    auto allowed_it = permanent_allowed_connections.find(first_socket_name);
                    
                    if (allowed_it == permanent_allowed_connections.end() ||
                        (allowed_it != permanent_allowed_connections.end() && 0 == allowed_it->second.count(second_socket_name)))
                    {
                        // connection allow/refuse chance will be
                        // checked only once during program work time
                        connection_allowed = beltpp::chance_one_of(chance_of_connect_base);

                        if (connection_allowed)
                        {
                            permanent_allowed_connections[first_socket_name].insert(second_socket_name);
                            permanent_allowed_connections[second_socket_name].insert(first_socket_name);
                        }
                        else
                        {
                            permanent_refused_connections[first_socket_name].insert(second_socket_name);
                            permanent_refused_connections[second_socket_name].insert(first_socket_name);
                        }
                    }
                }

                if (connection_allowed)
                {
                    // send join to peers
                    receive_send[first_socket_name][second_peer].emplace_back(beltpp::stream_join());
                    receive_send[second_socket_name][first_peer].emplace_back(beltpp::stream_join());

                    process_counter_state(first_socket_name, second_socket_name, true);
                    process_counter_state(second_socket_name, first_socket_name, true);
                }
                else
                {
                    // send refuse to peers
                    receive_send[first_socket_name][second_peer].emplace_back(beltpp::socket_open_refused());
                    receive_send[second_socket_name][first_peer].emplace_back(beltpp::socket_open_refused());

                    peers_to_drop.insert(first_peer);
                    peers_to_drop.insert(second_peer);
                }

                // fill associations for future use
                peer_to_peer.insert({ first_peer, second_peer });
                peer_to_peer.insert({ second_peer, first_peer });

                peer_to_ip.insert({ first_peer, second_address });
                peer_to_ip.insert({ second_peer, first_address });

                // close open attempt
                second_it = open_attempts.erase(second_it);

                break; // second for
            }
        }

        if (pair_found)
            first_it = open_attempts.erase(first_it);
        else
            ++first_it;
    }

    // connect all waiting listen/open pairs
    auto open_it = open_attempts.begin();
    for (; open_it != open_attempts.end();)
    {
        auto listen_it = listen_attempts.find(open_it->first.remote);

        if (listen_it == listen_attempts.end() ||
            listen_it->first.address != "test.brdhub.com") // rpc will work only on first node
            ++open_it;
        else
        {
            peer_id& open_peer = open_it->second;
            ip_address open_address = open_it->first;
            string& open_socket_name = peer_to_socket[open_peer];

            ip_address listen_address = listen_it->second.second;
            listen_address.remote = open_address.local;
            peer_id listen_peer = construct_peer_id(listen_address);
            string& listen_socket_name = peer_to_socket[listen_it->second.first];

            // create symmetric connections
            receive_send[open_socket_name][listen_peer].emplace_back(beltpp::stream_join());
            receive_send[listen_socket_name][open_peer].emplace_back(beltpp::stream_join());

            process_counter_state(open_socket_name, listen_socket_name, true);
            process_counter_state(listen_socket_name, open_socket_name, true);

            // fill associations for future use
            peer_to_peer.insert({ open_peer, listen_peer });
            peer_to_peer.insert({ listen_peer, open_peer });

            peer_to_ip.insert({ open_peer, listen_address });
            peer_to_ip.insert({ listen_peer, open_address });

            peer_to_socket.insert({ listen_peer, listen_socket_name });

            // close open attempt
            open_it = open_attempts.erase(open_it);
        }
    }
}

bool network_simulation::connection_closed(size_t const packet_type) const
{
    return (packet_type == beltpp::stream_drop::rtt) ||
           (packet_type == beltpp::stream_protocol_error::rtt) ||
           (packet_type == beltpp::socket_open_error::rtt) ||
           (packet_type == beltpp::socket_open_refused::rtt);
}

string network_simulation::export_connections(string socket_name)
{
    string result;

    for (auto const& item : receive_send)
        if (false == socket_name.empty() && item.first != socket_name)
            continue;
        else
        {
            list<string> tmp;
            for (auto const& it : item.second)
                if (0 == peers_to_drop.count(it.first))
                    tmp.push_back(peer_to_socket[it.first]);

            tmp.sort();
            result += item.first + " <=> ";
            for (auto it = tmp.begin(); it != tmp.end();)
            {
                result += *it;

                if (++it != tmp.end())
                    result += " ";
            }

            result += "\n";
        }

    return result;
}

string network_simulation::export_connections_matrix()
{
    string result;

    for (auto const& item : receive_send)
    {
        list<string> tmp;

        for (auto const& it : item.second)
        {
            bool to_drop = false;
            for (auto const& pack : it.second)
                if (connection_closed(pack.type()))
                {
                    to_drop = true;
                    break;
                }

            if (to_drop)
                continue;

            if (peer_to_socket.find(it.first) != peer_to_socket.end())
                tmp.push_back(peer_to_socket[it.first]);
            else
                tmp.push_back("oo"); // just for error detection
        }

        tmp.sort();
        result += item.first + " => ";
        result += format_index(tmp.size(), node_count, ' ') + " | ";
        result += format_index(permanent_refused_connections[item.first].size(), node_count, ' ') + " | ";

        size_t node_index = 0;
        for (auto it = tmp.begin(); it != tmp.end(); ++it)
        {
            while (node_index < node_count && *it != format_index(node_index , node_count))
            {
                ++node_index;
                result += "  ";
            }

            ++node_index;
            result += "**";
        }

        result += "\n";
    }

    return result;
}

string network_simulation::export_connections_load()
{
    string result;

    for (auto const& item : receive_send)
    {
        list<pair<string, size_t>> tmp;
        for (auto const& it : item.second)
            if (0 == peers_to_drop.count(it.first))
                tmp.push_back({ peer_to_socket[it.first], it.second.size() });

        tmp.sort();
        result += item.first + " <=> ";
        size_t node_index = 0;
        for (auto it = tmp.begin(); it != tmp.end();)
        {
            while (it->first != format_index(node_index, node_count))
            {
                ++node_index;
                result += "   ";
            }

            ++node_index;
            result += format_index(it->second, node_count);

            if (++it != tmp.end())
                result += " ";
        }

        result += "\n";
    }

    return result;
}

string network_simulation::export_connections_info()
{
    string result;

    result += "Active connections   : " + std::to_string(active_connections_count())   + "\n";
    //result += "Triangle connections : " + std::to_string(triangle_connections_count()) + "\n";

    return result;
}

string network_simulation::export_packets(const size_t rtt)
{
    string result;

    for (auto const& item : receive_send)
        for (auto const& it : item.second)
            for(auto const& pack : it.second)
                if (pack.type() == rtt)
                {
                    result += item.first + " < == > " + peer_to_socket[it.first] + "  ";

                    switch (rtt)
                    {
                    case beltpp::stream_join::rtt:
                    {
                        result += "join\n";
                        break;
                    }
                    case beltpp::stream_drop::rtt:
                    {
                        result += "drop\n";
                        break;
                    }
                    case beltpp::stream_protocol_error::rtt:
                    {
                        result += "protocol error\n";
                        break;
                    }
                    case beltpp::socket_open_refused::rtt:
                    {
                        result += "open refused\n";
                        break;
                    }
                    case beltpp::socket_open_error::rtt:
                    {
                        result += "open error\n";
                        break;
                    }
                    default:
                    {
                        try
                        {
                            auto models = BlockchainMessage::detail::meta_models();
                            string model_name = models.at(rtt);
                            result += model_name + "\n";
                        }
                        catch (std::out_of_range const& ex)
                        {
                            B_UNUSED(ex);
                            result += "unknown type! \n";
                        }
                        break;
                    }
                    }
                }

    return result;
}

size_t network_simulation::active_connections_count()
{
    size_t count = 0;

    for (auto const& receiver : receive_send)
        for (auto const& sender : receiver.second)
        {
            bool to_drop = false;
            for (auto const& pack : sender.second)
                if (connection_closed(pack.type()))
                {
                    to_drop = true;
                    break;
                }

            if (to_drop)
                continue;

            if (peer_to_peer.find(sender.first) != peer_to_peer.end())
                ++count;
        }

    return count / 2;
}

size_t network_simulation::triangle_connections_count()
{
    std::vector<std::vector<string>> triangles;

    for (auto const& receiver : receive_send_counter)
    {
        auto const& receiver_socket = receiver.first;
        auto const& receiver_connectios = receiver.second;

        for (auto const& sender : receiver_connectios)
        {
            auto const& sender_socket = sender.first;
            auto const& sender_connections = receive_send_counter[sender_socket];

            for (auto const& rc : receiver_connectios)
            {
                auto const& result = sender_connections.find(rc.first);

                if (result != sender_connections.end() &&
                    result->first != receiver_socket)
                {
                    auto const& third_socket = result->first;

                    auto const& third_connections = receive_send_counter[third_socket];

                    assert (third_connections.find(receiver_socket) != third_connections.end());
                    if (third_connections.find(receiver_socket) == third_connections.end())
                        throw std::logic_error("third_connections.find(receiver_socket) == third_connections.end()");

                    assert (third_connections.find(sender_socket) != third_connections.end());
                    if (third_connections.find(sender_socket) == third_connections.end())
                        throw std::logic_error("third_connections.find(sender_socket) == third_connections.end()");

                    assert (receiver_socket != sender_socket);
                    if (receiver_socket == sender_socket)
                        throw std::logic_error("receiver_socket == sender_socket");

                    assert (receiver_socket != third_socket);
                    if (receiver_socket == third_socket)
                        throw std::logic_error("receiver_socket == third_socket");

                    assert (sender_socket != third_socket);
                    if (sender_socket == third_socket)
                        throw std::logic_error("sender_socket == third_socket");

                    if (receiver_socket < sender_socket &&
                        receiver_socket < third_socket &&
                        sender_socket < third_socket)
                        triangles.push_back({receiver_socket, sender_socket, third_socket});
                }
            }
        }
    }

    return triangles.size();
}

string network_simulation::export_counter()
{
    string result;
    size_t total = 0;

    for (auto const& item : receive_send_counter)
    {
        result += item.first + " => ";
        result += format_index(item.second.size(), node_count, ' ') + "  ";
        result += format_index(permanent_refused_connections[item.first].size(), node_count, ' ') + " | ";

        string row;
        size_t count = 0;
        size_t node_index = 0;
        for (auto it = item.second.begin(); it != item.second.end(); ++it)
        {
            while (node_index < node_count && it->first != format_index(node_index, node_count))
            {
                ++node_index;
                row += "  ";
            }

            ++node_index;
            count += it->second;
            row += format_index(it->second, 99);
        }

        total += count;
        result += format_index(count, 99, ' ') + " | " + row + "\n";
    }

    result += "\nTotal messages : " + std::to_string(total) + "\n";

    return result;
}

void network_simulation::process_counter_state(string const& receiver, string const& sender, bool connect)
{
    if (connect)
        receive_send_counter[receiver][sender] = 0;
    else
        receive_send_counter[receiver].erase(sender);
}
//  event_handler_ns implementation

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

bool event_handler_ns::read()
{
    auto& my_sockets = m_ns->eh_to_sockets[this];
    for (auto const& item : m_ns->receive_send)
        if (my_sockets.count(item.first))
            for (auto const& it : item.second)
                if (it.second.size())
                    return true;

    return false;
}

event_handler::wait_result event_handler_ns::wait(std::unordered_set<event_item const*>& event_items)
{
    event_items.clear();

    for (auto& socket_name : m_ns->eh_to_sockets[this])
        if (nullptr == m_ns->name_to_sockets[socket_name].second)
            m_ns->name_to_sockets[socket_name].first->prepare_wait();
        else
            m_ns->name_to_sockets[socket_name].second->prepare_wait();

    if (m_timer_helper.expired())
    {
        m_timer_helper.update();
        return event_handler_ns::timer_out;
    }

    // check sent packets to my sockets
    auto& my_sockets = m_ns->eh_to_sockets[this];
    for (auto const& item : m_ns->receive_send)
        if (my_sockets.count(item.first))
            for (auto const& it : item.second)
                if (it.second.size())
                {
                    event_items.insert(m_ns->name_to_sockets[item.first].first);
                    break;
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
    m_timer_helper.set(period + std::chrono::steady_clock::duration(m_ns->timer_shuffle++));
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
    m_ns->receive_send.erase(m_ns->socket_to_name[&ev_it]);

    m_ns->socket_to_name.erase(&ev_it);
}

//  socket_ns implementation

socket_ns::socket_ns(event_handler_ns& eh, string& address, string name)
    : socket(eh)
    , m_name(name)
    , m_address(address)
    , m_ns(eh.m_ns)
{
    if (false == m_ns->socket_to_name.insert({ this, m_name }).second)
        throw std::logic_error("socket name is not unique!");

    if (false == m_ns->name_to_sockets.insert({ m_name, { this, nullptr } }).second)
        throw std::logic_error("socket name is not unique!");

    // chemistry support
    eh.last_socket_name = m_name;
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
        throw std::logic_error("ip is already listening : " + tmp_address.to_string());

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

    if (m_ns->open_attempts.find(address) != m_ns->open_attempts.end())
        throw std::runtime_error("ip address is already opening : " + address.to_string());

    bool no_connection = true;

    //check already active connection and reject open
    if (m_ns->receive_send.find(m_name) != m_ns->receive_send.end())
        for (auto& item : m_ns->receive_send[m_name])
            if (address == m_ns->peer_to_ip[item.first])
            {
                bool disconnect = false;
                for (auto const& pack : item.second)
                {
                    disconnect = m_ns->connection_closed(pack.type());

                    if (disconnect)
                        break;
                }

                if (disconnect)
                    break;
                else
                {
                    no_connection = false;
                    peers.emplace_back(item.first);
                }
            }

    if (no_connection)
    {
        peer_id peer = m_ns->construct_peer_id(address);

        m_ns->open_attempts.insert({ address, peer });
        m_ns->peer_to_socket.insert({ peer, m_name });

        peers.emplace_back(peer);
    }

    return peers;
}

void socket_ns::prepare_wait()
{
}

socket_ns::packets socket_ns::receive(peer_id& peer)
{
    auto my_buffers_it = m_ns->receive_send.find(m_name);
    if (my_buffers_it == m_ns->receive_send.end())
        throw std::logic_error("receive_packet() no any connection");

    bool disconnect = false;
    socket_ns::packets result;
    auto& my_buffers = my_buffers_it->second;

    // read first filled buffer and return
    for (auto& buffer : my_buffers)
        if (buffer.second.size())
        {
            peer = buffer.first;

            for (auto& pack : buffer.second)
            {
                disconnect = m_ns->connection_closed(pack.type());

                // broadcast calculation chemistry
                if (pack.type() == P2PMessage::Other::rtt)
                {
                    P2PMessage::Other wrap;
                    std::move(pack).get(wrap);

                    if (wrap.contents.type() == BlockchainMessage::Broadcast::rtt)
                        ++m_ns->receive_send_counter[m_name][m_ns->peer_to_socket[peer]];

                    pack.set(std::move(wrap));
                }

                result.emplace_back(std::move(pack));

                if (disconnect)
                    break;
            }

            buffer.second.clear();

            if (disconnect)
            {
                my_buffers.erase(peer);

                m_ns->peers_to_drop.insert(peer);
            }

            break;
        }

    return result;
}

void socket_ns::send(peer_id const& peer, beltpp::packet&& pack)
{
    auto receiver_socket_it = m_ns->peer_to_socket.find(peer);
    if (receiver_socket_it == m_ns->peer_to_socket.end())
        throw std::logic_error("send_packet() peer_to_socket association error");

    auto receiver_it = m_ns->receive_send.find(receiver_socket_it->second);
    if (receiver_it == m_ns->receive_send.end())
        throw std::logic_error("send_packet() no any connections");

    auto sender_peer_it = m_ns->peer_to_peer.find(peer);
    if (sender_peer_it == m_ns->peer_to_peer.end())
        throw std::logic_error("send_packet() peer_to_peer association error");

    if (receiver_it->second.find(sender_peer_it->second) == receiver_it->second.end())
        return; // connection was dropped, I will read the drop() packet soon
        //throw std::runtime_error(m_name + " send_packet() no connection with peer");

    if (pack.type() == beltpp::stream_drop::rtt)
    {
        // remove connection from peer to me
        auto my_buffers_it = m_ns->receive_send.find(m_name);
        if (my_buffers_it == m_ns->receive_send.end())
            throw std::logic_error("send_packet() my all connections are droped");

        auto peer_buffer_it = my_buffers_it->second.find(peer);
        if (peer_buffer_it == my_buffers_it->second.end())
            throw std::logic_error("send_packet() connection is already droped");

        my_buffers_it->second.erase(peer_buffer_it);

        m_ns->peers_to_drop.insert(peer);

        m_ns->process_counter_state(m_name, receiver_socket_it->second, false);
        m_ns->process_counter_state(receiver_socket_it->second, m_name, false);
    }

    //// broadcast calculation chemistry
    //if (pack.type() == P2PMessage::Other::rtt)
    //{
    //    P2PMessage::Other wrap;
    //    std::move(pack).get(wrap);
    //
    //    if (wrap.contents.type() == BlockchainMessage::Broadcast::rtt)
    //        ++m_ns->receive_send_counter[m_name][receiver_socket_it->second];
    //
    //    pack.set(std::move(wrap));
    //}

    auto& receiver_buffer = receiver_it->second[sender_peer_it->second];
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
