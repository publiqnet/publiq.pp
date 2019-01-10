#include "storage_node.hpp"
#include "common.hpp"
#include "storage_node_internals.hpp"

#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <exception>
#include <thread>

using namespace StorageMessage;

using beltpp::ip_address;
using beltpp::socket;
using beltpp::packet;
using peer_id = socket::peer_id;

namespace chrono = std::chrono;
using chrono::system_clock;

using std::pair;
using std::string;
using std::vector;
using std::unordered_set;

namespace publiqpp
{

/*
 * storage_node
 */
storage_node::storage_node(ip_address const& rpc_bind_to_address,
                           boost::filesystem::path const& fs_storage,
                           beltpp::ilog* plogger_storage_node,
                           bool log_enabled)
    : m_pimpl(new detail::storage_node_internals(rpc_bind_to_address,
                                                 fs_storage,
                                                 plogger_storage_node,
                                                 log_enabled))
{}

storage_node::storage_node(storage_node&&) noexcept = default;

storage_node::~storage_node() = default;

void storage_node::terminate()
{
    m_pimpl->m_ptr_eh->terminate();
}

bool storage_node::run()
{
    bool code = true;

    unordered_set<beltpp::ievent_item const*> wait_sockets;

    auto wait_result = m_pimpl->m_ptr_eh->wait(wait_sockets);

    if (wait_result == beltpp::event_handler::event)
    {
        for (auto& pevent_item : wait_sockets)
        {
            B_UNUSED(pevent_item);
            beltpp::socket::peer_id peerid;

            beltpp::isocket* psk = m_pimpl->m_ptr_rpc_socket.get();

            beltpp::socket::packets received_packets;
            if (psk != nullptr)
                received_packets = psk->receive(peerid);

            for (auto& received_packet : received_packets)
            {
            try
            {
                packet& ref_packet = received_packet;

                switch (ref_packet.type())
                {
                case beltpp::isocket_join::rtt:
                {
                    break;
                }
                case beltpp::isocket_drop::rtt:
                {
                    break;
                }
                case beltpp::isocket_protocol_error::rtt:
                {
                    break;
                }
                case beltpp::isocket_open_refused::rtt:
                {
                    beltpp::isocket_open_refused msg;
                    ref_packet.get(msg);
                    m_pimpl->writeln_node_warning(msg.reason + ", " + peerid);
                    break;
                }
                case beltpp::isocket_open_error::rtt:
                {
                    beltpp::isocket_open_error msg;
                    ref_packet.get(msg);
                    m_pimpl->writeln_node_warning(msg.reason + ", " + peerid);
                    break;
                }
                default:
                {
                    m_pimpl->writeln_node("don't know how to handle: " + std::to_string(ref_packet.type()) +
                                          ". dropping " + detail::peer_short_names(peerid));

                    psk->send(peerid, beltpp::isocket_drop());
                    break;
                }
                }   // switch ref_packet.type()
            }
            catch (std::exception const& e)
            {
                B_UNUSED(e);
                throw;
            }
            catch (...)
            {
                throw;
            }
            }   // for (auto& received_packet : received_packets)
        }   // for (auto& pevent_item : wait_sockets)
    }
    else if (beltpp::event_handler::timer_out == wait_result)
    {
        m_pimpl->m_ptr_rpc_socket->timer_action();
    }

    return code;
}

}


