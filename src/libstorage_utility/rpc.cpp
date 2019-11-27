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

#define NODES_TIME_SHIFT 60

namespace storage_utility
{
rpc::rpc(ip_address const& rpc_bind_to_address,
           beltpp::ilog* plogger_rpc)
    : m_pimpl(new detail::rpc_internals(rpc_bind_to_address,
                                        plogger_rpc))
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
                    m_pimpl->writeln_rpc("protocol error: ");
                    m_pimpl->writeln_rpc(msg.buffer);
                    psk->send(peerid, beltpp::packet(beltpp::isocket_drop()));

                    break;
                }
                case beltpp::isocket_open_refused::rtt:
                {
                    beltpp::isocket_open_refused msg;
                    received_packet.get(msg);
                    //m_pimpl->writeln_rpc_warning(msg.reason + ", " + peerid);
                    break;
                }
                case beltpp::isocket_open_error::rtt:
                {
                    beltpp::isocket_open_error msg;
                    received_packet.get(msg);
                    //m_pimpl->writeln_rpc_warning(msg.reason + ", " + peerid);
                    break;
                }
                case SignRequest::rtt:
                {
                    SignRequest msg_sign_request;
                    std::move(received_packet).get(msg_sign_request);

                    meshpp::private_key pv(msg_sign_request.private_key);

                    detail::rpc_internals::cache_key map_key;
                    map_key.authority_address = pv.get_public_key().to_string();
                    map_key.file_uri = msg_sign_request.order.file_uri;
                    map_key.content_unit_uri = msg_sign_request.order.content_unit_uri;
                    map_key.session_id = msg_sign_request.order.session_id;

                    auto it = m_pimpl->cache_signed_storage_order.find(map_key);

                    bool have_cache = false;
                    if (it != m_pimpl->cache_signed_storage_order.end())
                    {
                        auto expiry_time_point =
                                chrono::system_clock::from_time_t(it->second.order.time_point.tm) +
                                chrono::seconds(it->second.order.seconds);

                        if (expiry_time_point > std::chrono::system_clock::now() + chrono::seconds(NODES_TIME_SHIFT))
                            have_cache = true;
                    }

                    if (have_cache)
                        psk->send(peerid, beltpp::packet(it->second));
                    else
                    {
                        Authority authorization;
                        authorization.address = pv.get_public_key().to_string();
                        authorization.signature = pv.sign(msg_sign_request.order.to_string()).base58;

                        SignedStorageOrder signed_storage_order;
                        signed_storage_order.order = msg_sign_request.order;
                        signed_storage_order.authorization = authorization;

                        m_pimpl->cache_signed_storage_order[map_key] = signed_storage_order;

                        psk->send(peerid, beltpp::packet(std::move(signed_storage_order)));
                    }

                    break;
                }
                case SignedStorageOrder::rtt:
                {
                    SignedStorageOrder msg_verfy_sig_storage_order;
                    std::move(received_packet).get(msg_verfy_sig_storage_order);

                    bool correct = meshpp::verify_signature(
                                       msg_verfy_sig_storage_order.authorization.address,
                                       msg_verfy_sig_storage_order.order.to_string(),
                                       msg_verfy_sig_storage_order.authorization.signature);

                    if (correct)
                    {
                        VerificationResponse verify_response;

                        verify_response.address = msg_verfy_sig_storage_order.authorization.address;
                        verify_response.storage_order = std::move(msg_verfy_sig_storage_order.order);

                        psk->send(peerid, beltpp::packet(std::move(verify_response)));
                    }
                    else
                    {
                        throw std::runtime_error("invalid signature");
                    }

                    break;
                }
                default:
                {
                    m_pimpl->writeln_rpc("can't handle: " + std::to_string(received_packet.type()) +
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
//            catch (wrong_data_exception const& e)
//            {
//                RemoteError remote_error;
//                remote_error.message = e.message;
//                psk->send(peerid, beltpp::packet(remote_error));

//                throw;
//            }
//            catch (wrong_request_exception const& e)
//            {
//                RemoteError remote_error;
//                remote_error.message = e.message;
//                psk->send(peerid, beltpp::packet(remote_error));

//                throw;
//            }
//            catch (wrong_document_exception const& e)
//            {
//                RemoteError remote_error;
//                remote_error.message = e.message;
//                psk->send(peerid, beltpp::packet(remote_error));

//                throw;
//            }
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
    {
        auto it = m_pimpl->cache_signed_storage_order.begin();
        while (it != m_pimpl->cache_signed_storage_order.end())
        {
            auto expiry_time_point =
                    chrono::system_clock::from_time_t(it->second.order.time_point.tm) +
                    chrono::seconds(it->second.order.seconds);

            if (expiry_time_point <= std::chrono::system_clock::now() + chrono::seconds(NODES_TIME_SHIFT))
                it = m_pimpl->cache_signed_storage_order.erase(it);
            else
                ++it;
        }

        m_pimpl->m_ptr_rpc_socket->timer_action();
    }

    if (wait_result & beltpp::event_handler::on_demand)
    {
    }

    return code;
}
}


