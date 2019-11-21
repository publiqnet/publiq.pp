#include "rpc.hpp"
#include "exception.hpp"
#include "rpc_internals.hpp"

#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <exception>

using beltpp::packet;
using beltpp::socket;
using beltpp::ip_address;
using peer_id = socket::peer_id;


namespace chrono = std::chrono;
using chrono::system_clock;

using std::pair;
using std::string;
using std::vector;
using std::unique_ptr;
using std::unordered_set;

namespace storage_utility
{

rpc::rpc(ip_address const& rpc_bind_to_address,
           beltpp::ilog* plogger_node)
    : m_pimpl(new detail::rpc_internals(rpc_bind_to_address,
                                        plogger_node))
{}

rpc::rpc(rpc&&) noexcept = default;

rpc::~rpc() = default;

void rpc::wake()
{
    m_pimpl->m_ptr_eh->wake();
}

bool rpc::run()
{
    bool code = true;

    unordered_set<beltpp::ievent_item const*> wait_sockets;

    auto wait_result = m_pimpl->m_ptr_eh->wait(wait_sockets);

    if (wait_result & beltpp::event_handler::event)
    {
        for (auto& pevent_item : wait_sockets)
        {
            beltpp::socket::peer_id peerid;

            beltpp::isocket* psk = nullptr;
            if (pevent_item == m_pimpl->m_ptr_rpc_socket.get())
                psk = m_pimpl->m_ptr_rpc_socket.get();

            if (nullptr == psk)
                throw std::logic_error("event handler behavior");

            beltpp::socket::packets received_packets;
            if (psk != nullptr)
                received_packets = psk->receive(peerid);

            for (auto& received_packet : received_packets)
            {
            try
            {
                switch (received_packet.type())
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
                    beltpp::isocket_protocol_error msg;
                    received_packet.get(msg);
                    m_pimpl->writeln_node("protocol error: ");
                    m_pimpl->writeln_node(msg.buffer);
                    psk->send(peerid, beltpp::packet(beltpp::isocket_drop()));

                    break;
                }
                case beltpp::isocket_open_refused::rtt:
                {
                    beltpp::isocket_open_refused msg;
                    received_packet.get(msg);
                    //m_pimpl->writeln_node_warning(msg.reason + ", " + peerid);
                    break;
                }
                case beltpp::isocket_open_error::rtt:
                {
                    beltpp::isocket_open_error msg;
                    received_packet.get(msg);
                    //m_pimpl->writeln_node_warning(msg.reason + ", " + peerid);
                    break;
                }
                default:
                {
                    //if (received_packet.type() != SyncResponse::rtt)
                        m_pimpl->writeln_node("master can't handle: " + std::to_string(received_packet.type()) +
                                              ". peer: " + peerid);

                    break;
                }
                }   // switch received_packet.type()
            }
//            catch (meshpp::exception_public_key const& e)
//            {
//                InvalidPublicKey msg;
//                msg.public_key = e.pub_key;
//                psk->send(peerid, beltpp::packet(msg));

//                throw;
//            }
//            catch (meshpp::exception_private_key const& e)
//            {
//                InvalidPrivateKey msg;
//                msg.private_key = e.priv_key;
//                psk->send(peerid, beltpp::packet(msg));

//                throw;
//            }
//            catch (meshpp::exception_signature const& e)
//            {
//                InvalidSignature msg;
//                msg.details.public_key = e.sgn.pb_key.to_string();
//                msg.details.signature = e.sgn.base58;
//                BlockchainMessage::detail::loader(msg.details.package,
//                                                  std::string(e.sgn.message.begin(), e.sgn.message.end()),
//                                                  nullptr);

//                psk->send(peerid, beltpp::packet(msg));
//                throw;
//            }
            catch (wrong_data_exception const& e)
            {
                RemoteError remote_error;
                remote_error.message = e.message;
                psk->send(peerid, beltpp::packet(remote_error));

                throw;
            }
            catch (wrong_request_exception const& e)
            {
                RemoteError remote_error;
                remote_error.message = e.message;
                psk->send(peerid, beltpp::packet(remote_error));

                throw;
            }
            catch (wrong_document_exception const& e)
            {
                RemoteError remote_error;
                remote_error.message = e.message;
                psk->send(peerid, beltpp::packet(remote_error));

                throw;
            }
//            catch (authority_exception const& e)
//            {
//                InvalidAuthority msg;
//                msg.authority_provided = e.authority_provided;
//                msg.authority_required = e.authority_required;
//                psk->send(peerid, beltpp::packet(msg));

//                throw;
//            }
//            catch (too_long_string_exception const& e)
//            {
//                TooLongString msg;
//                beltpp::assign(msg, e);
//                psk->send(peerid, beltpp::packet(msg));

//                throw;
//            }
//            catch (uri_exception const& e)
//            {
//                UriError msg;
//                beltpp::assign(msg, e);
//                psk->send(peerid, beltpp::packet(msg));

//                throw;
//            }
            catch (std::exception const& e)
            {
                RemoteError msg;
                msg.message = e.what();
                psk->send(peerid, beltpp::packet(msg));

                throw;
            }
            catch (...)
            {
                RemoteError msg;
                msg.message = "unknown exception";
                psk->send(peerid, beltpp::packet(msg));

                throw;
            }
            }   // for (auto& received_packet : received_packets)
        }   // for (auto& pevent_item : wait_sockets)
    }

    if (wait_result & beltpp::event_handler::timer_out)
        m_pimpl->m_ptr_rpc_socket->timer_action();

    if (wait_result & beltpp::event_handler::on_demand)
    {
    }

    return code;
}
}


