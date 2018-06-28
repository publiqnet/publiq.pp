#include "node.hpp"
#include "state.hpp"
#include "blockchain.hpp"
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
#include <stack>

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

//  MSVS does not instansiate template function only because its address
//  is needed, so let's force it
template beltpp::void_unique_ptr beltpp::new_void_unique_ptr<Error>();
template beltpp::void_unique_ptr beltpp::new_void_unique_ptr<Join>();
template beltpp::void_unique_ptr beltpp::new_void_unique_ptr<Drop>();

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

namespace http
{
class scan_status
{
public:
    enum e_status {clean, http_request_progress, http_properties_progress, http_done};
    scan_status()
        : status(clean)
    {}
    e_status status;
};

vector<char> save_header(beltpp::detail::session_special_data& ssd,
                         vector<char> const& message_stream)
{
    ssd.session_specal_handler = nullptr;
    string str_result;
    str_result += "HTTP/1.1 200 OK\r\n";
    str_result += "Content-Type: application/json\r\n";
    str_result += "Content-Length: ";
    str_result += std::to_string(message_stream.size());
    str_result += "\r\n\r\n";

    return vector<char>(str_result.begin(), str_result.end());
}

beltpp::iterator_wrapper<char const>
check_begin(beltpp::iterator_wrapper<char const> const& iter_scan_begin,
            beltpp::iterator_wrapper<char const> const& iter_scan_end,
            string const& value) noexcept
{
    auto it_scan = iter_scan_begin;
    auto it_value = value.begin();

    while (true)
    {
        if (it_scan == iter_scan_end ||
            it_value == value.end())
            break;
        if (*it_value == *it_scan)
        {
            ++it_value;
            ++it_scan;
        }
        else
        {
            it_scan = iter_scan_begin;
            break;
        }
    }

    return it_scan;
}

beltpp::iterator_wrapper<char const>
check_end(beltpp::iterator_wrapper<char const> const& iter_scan_begin,
          beltpp::iterator_wrapper<char const> const& iter_scan_end,
          string const& value,
          bool& full) noexcept
{
    full = false;
    auto it_scan_begin = iter_scan_begin;
    auto it_scan = it_scan_begin;
    auto it_value = value.begin();

    while (true)
    {
        if (it_value == value.end())
        {
            full = true;
            break;
        }
        if (it_value != value.end() &&
            it_scan == iter_scan_end)
        {
            it_scan = iter_scan_begin;
            break;
        }
        if (*it_value == *it_scan)
        {
            ++it_value;
            ++it_scan;
        }
        else
        {
            it_value = value.begin();
            ++it_scan_begin;
            it_scan = it_scan_begin;
        }
    }

    return it_scan;
}

beltpp::detail::pmsg_all message_list_load(
        beltpp::iterator_wrapper<char const>& iter_scan_begin,
        beltpp::iterator_wrapper<char const> const& iter_scan_end,
        beltpp::detail::session_special_data& ssd,
        void* putl)
{
    if (nullptr == ssd.ptr_data)
        ssd.ptr_data = beltpp::new_void_unique_ptr<scan_status>();
    auto it_backup = iter_scan_begin;

    size_t http_header_scanning = 0;

    scan_status& ss = *reinterpret_cast<scan_status*>(ssd.ptr_data.get());
    if (scan_status::clean == ss.status ||
        scan_status::http_done == ss.status)
    {
        string value_check = "POST ";
        auto iter_scan = check_begin(iter_scan_begin, iter_scan_end, value_check);
        if (iter_scan_begin != iter_scan)
        {
            string temp(iter_scan_begin, iter_scan);
            //  even if "P" occured switch to http mode
            ss.status = scan_status::http_request_progress;
        }
    }
    if (scan_status::http_request_progress == ss.status)
    {
        string value_check = "POST ";
        auto iter_scan1 = check_begin(iter_scan_begin, iter_scan_end, value_check);
        if (value_check == string(iter_scan_begin, iter_scan1))
        {
            bool full = false;
            auto iter_scan2 = check_end(iter_scan_begin, iter_scan_end, "\r\n", full);

            string temp(iter_scan_begin, iter_scan2);
            http_header_scanning += temp.length();

            if (full)
            {
                iter_scan_begin = iter_scan2;
                ss.status = scan_status::http_properties_progress;
            }
        }
    }
    while (scan_status::http_properties_progress == ss.status &&
           http_header_scanning < 1024 * 64)    //  don't support http header bigger than 64kb
    {
        bool full = false;
        auto iter_scan2 = check_end(iter_scan_begin, iter_scan_end, "\r\n", full);

        string temp(iter_scan_begin, iter_scan2);
        http_header_scanning += temp.length();

        if (full)
        {
            iter_scan_begin = iter_scan2;
            if (temp.length() == 2)
            {
                ss.status = scan_status::http_done;
                ssd.session_specal_handler = &save_header;
            }
        }
        else
            break;
    }

    if (http_header_scanning >= 64 * 1024)
    {
        ss.status = scan_status::clean;
        iter_scan_begin = iter_scan_end;
        return ::beltpp::detail::pmsg_all(0,
                                          ::beltpp::void_unique_ptr(nullptr, [](void*){}),
                                          nullptr);
    }
    else if (scan_status::http_properties_progress == ss.status ||
             scan_status::http_request_progress == ss.status)
    {
        //  revert the cursor, so everything will be rescanned
        //  once there is more data to scan
        //  in future may implement persistent state, so rescan will not
        //  be needed
        ss.status = scan_status::clean;
        iter_scan_begin = it_backup;
        return ::beltpp::detail::pmsg_all(size_t(-1),
                                          ::beltpp::void_unique_ptr(nullptr, [](void*){}),
                                          nullptr);
    }
    else
        return BlockchainMessage::message_list_load(iter_scan_begin, iter_scan_end, ssd, putl);
}
}

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
    &http::message_list_load
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
                   boost::filesystem::path const& fs_action_log,
                   boost::filesystem::path const& fs_storage,
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
        , m_state(fs_blockchain, fs_action_log, fs_storage)
    {
        m_ptr_eh->set_timer(chrono::seconds(1));

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
    publiqpp::state m_state;

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
           boost::filesystem::path const& fs_action_log,
           boost::filesystem::path const& fs_storage,
           beltpp::ilog* plogger_p2p,
           beltpp::ilog* plogger_node)
    : m_pimpl(new detail::node_internals(rpc_bind_to_address,
                                         p2p_bind_to_address,
                                         p2p_connect_to_addresses,
                                         fs_blockchain,
                                         fs_action_log,
                                         fs_storage,
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

                        packets.emplace_back(std::move(container.value));
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
                case Shutdown::rtt:
                {
                    m_pimpl->writeln_node("shutdown received");

                    code = false;

                    Hellow hellow_msg;
                    hellow_msg.index = 0;
                    hellow_msg.text = "bye";
                    psk->send(peerid, hellow_msg);

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
                case GetChainInfo::rtt:
                {
                    ChainInfo chaininfo_msg;
                    chaininfo_msg.length = m_pimpl->m_state.blockchain().length();
                    psk->send(peerid, chaininfo_msg);
                    break;
                }
                case SubmitActions::rtt:
                {
                    bool answered = false;
                    if (it == interface_type::rpc)
                    {
                        try
                        {
                            SubmitActions submitactions_msg;
                            ref_packet.get(submitactions_msg);

                            if (Reward::rtt == submitactions_msg.item.type())
                            {   //  check reward, account and coin rtts for testing
                                Reward msg_reward;
                                submitactions_msg.item.get(msg_reward);

                                m_pimpl->m_state.action_log().insert(msg_reward);
                                psk->send(peerid, Done());
                            }
                            else if (RevertLastAction::rtt == submitactions_msg.item.type())
                            {   //  pay attention - RevertLastAction is sent, but RevertActionAt is stored

                                // check if last action is revert
                                int revert_mark = 0;
                                size_t index = m_pimpl->m_state.action_log().length() - 1;
                                bool revert = index > 0;

                                while (revert)
                                {
                                    beltpp::packet packet;
                                    m_pimpl->m_state.action_log().at(index, packet);

                                    revert = packet.type() == RevertActionAt::rtt;

                                    if (revert)
                                        ++revert_mark;
                                    else
                                        --revert_mark;

                                    if (revert_mark >= 0)
                                        --index;
                                    else
                                        revert = false;
                                }

                                // revert last valid action
                                beltpp::packet packet;
                                m_pimpl->m_state.action_log().at(index, packet);

                                RevertActionAt msg_revert;
                                msg_revert.index = index;
                                msg_revert.item = std::move(packet);

                                m_pimpl->m_state.action_log().insert(msg_revert);
                                psk->send(peerid, Done());
                            }
                            else if (NewArticle::rtt == submitactions_msg.item.type())
                            {
                                NewArticle msg_new_article;
                                submitactions_msg.item.get(msg_new_article);

                                m_pimpl->m_state.action_log().insert(msg_new_article);
                                psk->send(peerid, Done());
                            }
                        }
                        catch (std::exception const& ex)
                        {
                            if (false == answered)
                            {
                                Failed msg_failed;
                                msg_failed.message = ex.what();
                                psk->send(peerid, msg_failed);
                            }
                        }
                        catch (...)
                        {
                            if (false == answered)
                            {
                                Failed msg_failed;
                                msg_failed.message = "unknown exception";
                                psk->send(peerid, msg_failed);
                            }
                        }
                    }
                    break;
                }
                case GetActions::rtt:
                {
                    GetActions msg_get_actions;
                    ref_packet.get(msg_get_actions);
                    uint64_t index = msg_get_actions.start_index;

                    std::stack<Action> action_stack;

                    size_t i = index;
                    size_t len = m_pimpl->m_state.action_log().length();

                    bool revert = i < len;
                    while (revert) //the case when next action is revert
                    {
                        beltpp::packet packet;
                        m_pimpl->m_state.action_log().at(i, packet);

                        revert = packet.type() == RevertActionAt::rtt;

                        if (revert)
                        {
                            ++i;
                            Action action;
                            action.index = i;
                            action.item = std::move(packet);

                            action_stack.push(action);
                        }
                    }

                    for (; i < len; ++i)
                    {
                        beltpp::packet packet;
                        m_pimpl->m_state.action_log().at(i, packet);

                        // remove all not received entries and their reverts
                        revert = packet.type() == RevertActionAt::rtt;
                        if (revert)
                        {
                            RevertActionAt msg;
                            packet.get(msg);

                            revert = msg.index >= index;
                        }

                        if (revert) 
                        {
                            action_stack.pop();
                        }
                        else
                        {

                            Action action;
                            action.index = i;
                            action.item = std::move(packet);

                            action_stack.push(action);
                        }
                    }

                    std::stack<Action> reverse_stack;
                    while (!action_stack.empty()) // revers the stack
                    {
                        reverse_stack.push(action_stack.top());
                        action_stack.pop();
                    }

                    Actions msg_actions;
                    while(!reverse_stack.empty()) // list is a vector in reality :)
                    {
                        msg_actions.list.push_back(std::move(reverse_stack.top()));
                        reverse_stack.pop();
                    }

                    psk->send(peerid, msg_actions);
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


