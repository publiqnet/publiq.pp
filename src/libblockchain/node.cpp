#include "node.hpp"
#include "state.hpp"
#include "message.hpp"

#include <belt.pp/packet.hpp>
#include <belt.pp/utility.hpp>
#include <belt.pp/event.hpp>

#include <belt.pp/socket.hpp>
#include <belt.pp/packet.hpp>
#include <belt.pp/message_global.hpp>

#include <mesh.pp/p2psocket.hpp>

#include <exception>
#include <string>
#include <memory>
#include <chrono>
#include <unordered_set>
#include <vector>

using namespace BlockchainMessage;

using beltpp::ip_address;
using beltpp::ip_destination;
using beltpp::socket;
using beltpp::packet;
using peer_id = socket::peer_id;
using std::unordered_set;

namespace chrono = std::chrono;
using chrono::system_clock;
using chrono::steady_clock;
using std::string;
using std::vector;
using std::unique_ptr;

namespace publiqpp
{

using p2p_sf = meshpp::p2psocket_family_t<
    Error::rtt,
    Join::rtt,
    Drop::rtt,
    &beltpp::new_void_unique_ptr<Error>,
    &beltpp::new_void_unique_ptr<Join>,
    &beltpp::new_void_unique_ptr<Drop>,
    &Error::saver,
    &Join::saver,
    &Drop::saver
>;


using rpc_sf = beltpp::socket_family_t<
    Error::rtt,
    Join::rtt,
    Drop::rtt,
    &beltpp::new_void_unique_ptr<Error>,
    &beltpp::new_void_unique_ptr<Join>,
    &beltpp::new_void_unique_ptr<Drop>,
    &Error::saver,
    &Join::saver,
    &Drop::saver,
    &message_list_load
>;

namespace detail
{
beltpp::void_unique_ptr get_putl()
{
    beltpp::message_loader_utility utl;
    BlockchainMessage::detail::extension_helper(utl);

    auto ptr_utl =
            beltpp::new_void_unique_ptr<beltpp::message_loader_utility>(std::move(utl));

    return ptr_utl;
}
class node_internals
{
public:
    node_internals(ip_address const& rpc_bind_to_address,
                   ip_address const& p2p_bind_to_address,
                   std::vector<ip_address> const& p2p_connect_to_addresses,
                   boost::filesystem::path const& fs_blockchain,
                   beltpp::ilog* _plogger_p2p,
                   beltpp::ilog* _plogger_node)
        : plogger_p2p(_plogger_p2p)
        , plogger_node(_plogger_node)
        , m_ptr_eh(new beltpp::event_handler())
        , m_ptr_p2p_socket(new meshpp::p2psocket(
                               meshpp::getp2psocket<p2p_sf>(*m_ptr_eh,
                                                            p2p_bind_to_address,
                                                            p2p_connect_to_addresses,
                                                            get_putl(),
                                                            _plogger_p2p)
                               ))
        , m_ptr_rpc_socket(new beltpp::socket(
                               beltpp::getsocket<rpc_sf>(*m_ptr_eh)
                               ))
        , m_ptr_state(publiqpp::getstate(fs_blockchain))
    {
        m_ptr_eh->set_timer(chrono::seconds(10));

        m_ptr_rpc_socket->listen(rpc_bind_to_address);

        m_ptr_eh->add(*m_ptr_rpc_socket);
        m_ptr_eh->add(*m_ptr_p2p_socket);
    }

    void write_p2p(string const& value)
    {
        if (plogger_p2p)
            plogger_p2p->message_no_eol(value);
    }

    void writeln_p2p(string const& value)
    {
        if (plogger_p2p)
            plogger_p2p->message(value);
    }

    void write_node(string const& value)
    {
        if (plogger_node)
            plogger_node->message_no_eol(value);
    }

    void writeln_node(string const& value)
    {
        if (plogger_node)
            plogger_node->message(value);
    }

    beltpp::ilog* plogger_p2p;
    beltpp::ilog* plogger_node;
    unique_ptr<beltpp::event_handler> m_ptr_eh;
    unique_ptr<meshpp::p2psocket> m_ptr_p2p_socket;
    unique_ptr<beltpp::socket> m_ptr_rpc_socket;
    publiqpp::state_ptr m_ptr_state;

    unordered_set<string> p2p_peers;
};
}

/*
 * node
 */
node::node(ip_address const& rpc_bind_to_address,
           ip_address const& p2p_bind_to_address,
           std::vector<ip_address> const& p2p_connect_to_addresses,
           boost::filesystem::path const& fs_blockchain,
           beltpp::ilog* plogger_p2p,
           beltpp::ilog* plogger_node)
    : m_pimpl(new detail::node_internals(rpc_bind_to_address,
                                         p2p_bind_to_address,
                                         p2p_connect_to_addresses,
                                         fs_blockchain,
                                         plogger_p2p,
                                         plogger_node))
{

}

node::node(node&&) = default;

node::~node()
{

}

/*void node::send(peer_id const& peer,
                packet&& pack)
{
    Other wrapper;
    wrapper.contents = std::move(pack);
    m_pimpl->m_ptr_p2p_socket->send(peer, std::move(wrapper));
}*/

string node::name() const
{
    return m_pimpl->m_ptr_p2p_socket->name();
}

bool node::run()
{
    bool code = true;

    unordered_set<beltpp::ievent_item const*> wait_sockets;

    //m_pimpl->writeln_node("eh.wait");
    auto wait_result = m_pimpl->m_ptr_eh->wait(wait_sockets);
    //m_pimpl->writeln_node("eh.wait - done");

    enum class interface_type {p2p, rpc};

    if (wait_result == beltpp::event_handler::event)
    {
        for (auto& pevent_item : wait_sockets)
        {
            interface_type it = interface_type::rpc;
            if (pevent_item == &m_pimpl->m_ptr_p2p_socket.get()->worker())
                it = interface_type::p2p;

            auto str_receive = [it]
            {
                if (it == interface_type::p2p)
                    return "p2p_sk.receive";
                else
                    return "rpc_sk.receive";
            };
            str_receive();

            auto str_peerid = [it](string const& peerid)
            {
                if (it == interface_type::p2p)
                    return peerid.substr(0, 5);
                else
                    return peerid;
            };

            beltpp::socket::peer_id peerid;

            beltpp::isocket* psk = nullptr;
            if (pevent_item == &m_pimpl->m_ptr_p2p_socket->worker())
                psk = m_pimpl->m_ptr_p2p_socket.get();
            else if (pevent_item == m_pimpl->m_ptr_rpc_socket.get())
                psk = m_pimpl->m_ptr_rpc_socket.get();

            beltpp::socket::packets received_packets;
            if (psk)
            {
                //m_pimpl->writeln_node(str_receive());
                received_packets = psk->receive(peerid);
                //m_pimpl->writeln_node("done");
            }


            for (auto& received_packet : received_packets)
            {
                bool broadcast_packet = false;

                vector<packet> packets;
                packets.emplace_back(std::move(received_packet));

                while (true)
                {
                    bool container_type = false;
                    packet& ref_packet = packets.back();

                    switch (ref_packet.type())
                    {
                    case Broadcast::rtt:
                    {
                        container_type = true;
                        broadcast_packet = true;
                        Broadcast container;
                        ref_packet.get(container);

                        packets.emplace_back(std::move(container.payload));
                        break;
                    }
                    }

                    if (false == container_type)
                        break;
                }

                packet& ref_packet = packets.back();

                switch (ref_packet.type())
                {
                case Join::rtt:
                {
                    m_pimpl->write_node(str_peerid(peerid));
                    m_pimpl->writeln_node("joined");

                    if (psk == m_pimpl->m_ptr_p2p_socket.get())
                        m_pimpl->p2p_peers.insert(peerid);

                    break;
                }
                case Drop::rtt:
                {
                    m_pimpl->write_node(str_peerid(peerid));
                    m_pimpl->writeln_node("dropped");

                    if (psk == m_pimpl->m_ptr_p2p_socket.get())
                        m_pimpl->p2p_peers.erase(peerid);

                    break;
                }
                case Error::rtt:
                {
                    m_pimpl->write_node(str_peerid(peerid));
                    m_pimpl->writeln_node("error");
                    psk->send(peerid, Drop());

                    if (psk == m_pimpl->m_ptr_p2p_socket.get())
                        m_pimpl->p2p_peers.erase(peerid);

                    break;
                }
                case Hellow::rtt:
                {
                    Hellow hellow_msg;
                    ref_packet.get(hellow_msg);

                    if (hellow_msg.index % 1000 == 0)
                    {
                        m_pimpl->write_node("Hellow:");
                        m_pimpl->writeln_node(hellow_msg.text);
                        m_pimpl->write_node("From:");
                        m_pimpl->writeln_node(str_peerid(peerid));
                    }

                    if (broadcast_packet)
                    {
                        if (hellow_msg.index % 1000 == 0)
                            m_pimpl->writeln_node("broadcasting hellow");

                        for (auto const& p2p_peer : m_pimpl->p2p_peers)
                            m_pimpl->m_ptr_p2p_socket->send(p2p_peer, hellow_msg);
                    }
                    break;
                }
                case Broadcast::rtt:
                {
                    Broadcast broadcast_msg;
                    ref_packet.get(broadcast_msg);

                    Hellow hellow_msg;
                    if (broadcast_msg.payload.type() != Hellow::rtt)
                        break;
                    broadcast_msg.payload.get(hellow_msg);

                    break;
                }
                case Shutdown::rtt:
                {
                    m_pimpl->writeln_node("shutdown received");

                    code = false;

                    if (broadcast_packet)
                    {
                        m_pimpl->writeln_node("broadcasting shutdown");

                        Shutdown shutdown_msg;
                        ref_packet.get(shutdown_msg);

                        for (auto const& p2p_peer : m_pimpl->p2p_peers)
                            m_pimpl->m_ptr_p2p_socket->send(p2p_peer, shutdown_msg);
                    }
                    break;
                }
                }
            }
        }
    }
    else if (beltpp::event_handler::timer_out == wait_result)
    {
        m_pimpl->writeln_node("timer");

        m_pimpl->m_ptr_p2p_socket->timer_action();
        m_pimpl->m_ptr_rpc_socket->timer_action();
    }

    return code;
}

}


