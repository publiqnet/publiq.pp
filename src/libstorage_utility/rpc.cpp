#include "rpc.hpp"
#include "exception.hpp"
#include "rpc_internals.hpp"

#include <vector>
#include <string>
#include <memory>
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

    unordered_set<beltpp::event_item const*> wait_sockets;

    auto wait_result = m_pimpl->m_ptr_eh->wait(wait_sockets);

    if (wait_result & beltpp::event_handler::event)
    {
        for (auto& pevent_item : wait_sockets)
        {
            beltpp::socket::peer_id peerid;

            beltpp::stream* psk = nullptr;
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
                case beltpp::stream_join::rtt:
                {

                    break;
                }
                case beltpp::stream_drop::rtt:
                {

                    break;
                }
                case beltpp::stream_protocol_error::rtt:
                {
                    beltpp::stream_protocol_error msg;
                    received_packet.get(msg);
                    m_pimpl->writeln_rpc("protocol error: ");
                    m_pimpl->writeln_rpc(msg.buffer);
                    psk->send(peerid, beltpp::packet(beltpp::stream_drop()));

                    break;
                }
                case beltpp::socket_open_refused::rtt:
                {
                    beltpp::socket_open_refused msg;
                    received_packet.get(msg);
                    //m_pimpl->writeln_rpc_warning(msg.reason + ", " + peerid);
                    break;
                }
                case beltpp::socket_open_error::rtt:
                {
                    beltpp::socket_open_error msg;
                    received_packet.get(msg);
                    //m_pimpl->writeln_rpc_warning(msg.reason + ", " + peerid);
                    break;
                }
                case SignRequest::rtt:
                {
                    SignRequest msg_sign_request;
                    std::move(received_packet).get(msg_sign_request);

                    auto now = std::chrono::system_clock::now();
                    meshpp::private_key pv(msg_sign_request.private_key);

                    if (msg_sign_request.order.seconds != SIGN_SECONDS)
                        throw std::runtime_error("can sign only for 3600 seconds");
                    auto requested_time_point = chrono::system_clock::from_time_t(msg_sign_request.order.time_point.tm);
                    if (requested_time_point > now + chrono::seconds(NODES_TIME_SHIFT) ||
                        requested_time_point < now - chrono::seconds(NODES_TIME_SHIFT))
                        throw std::runtime_error("out of sync");

                    detail::rpc_internals::cache_key map_key;
                    map_key.authority_address = pv.get_public_key().to_string();
                    map_key.file_uri = msg_sign_request.order.file_uri;
                    map_key.content_unit_uri = msg_sign_request.order.content_unit_uri;
                    map_key.session_id = msg_sign_request.order.session_id;

                    auto it = m_pimpl->cache_signed_storage_order.find(map_key);

                    bool have_cache = false;
                    if (it != m_pimpl->cache_signed_storage_order.end())
                    {
                        auto expiring_time_point =
                                chrono::system_clock::from_time_t(it->second.order.time_point.tm) +
                                chrono::seconds(it->second.order.seconds);

                        if (expiring_time_point > now + chrono::seconds(NODES_TIME_SHIFT))
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

                    if (msg_verfy_sig_storage_order.order.seconds != SIGN_SECONDS)
                        throw std::runtime_error("can sign only for 3600 seconds");

                    auto now = chrono::system_clock::now();
                    auto signed_time_point = chrono::system_clock::from_time_t(msg_verfy_sig_storage_order.order.time_point.tm);
                    auto expiring_time_point = signed_time_point + chrono::seconds(msg_verfy_sig_storage_order.order.seconds);

                    if (signed_time_point > now + chrono::seconds(NODES_TIME_SHIFT) ||
                        expiring_time_point <= now - chrono::seconds(NODES_TIME_SHIFT))
                        throw std::runtime_error("out of sync");

                    bool correct = meshpp::verify_signature(
                                       msg_verfy_sig_storage_order.authorization.address,
                                       msg_verfy_sig_storage_order.order.to_string(),
                                       msg_verfy_sig_storage_order.authorization.signature);

                    if (false == correct)
                        throw std::runtime_error("invalid signature");

                    VerificationResponse verify_response;

                    verify_response.address = msg_verfy_sig_storage_order.authorization.address;
                    verify_response.storage_order = std::move(msg_verfy_sig_storage_order.order);

                    psk->send(peerid, beltpp::packet(std::move(verify_response)));

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
            auto now = std::chrono::system_clock::now();
            auto expiring_time_point =
                    chrono::system_clock::from_time_t(it->second.order.time_point.tm) +
                    chrono::seconds(it->second.order.seconds);

            if (expiring_time_point <= now - chrono::seconds(NODES_TIME_SHIFT))
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

bool rpc::verify_storage_order(string const& storage_order_token,
                               string& channel_address,
                               string& storage_address,
                               string& file_uri,
                               string& content_unit_uri,
                               string& session_id,
                               uint64_t& seconds,
                               chrono::system_clock::time_point& tp)
{
    SignedStorageOrder msg_verfy_sig_storage_order;

    msg_verfy_sig_storage_order.from_string(meshpp::from_base64(storage_order_token), nullptr);

    channel_address = msg_verfy_sig_storage_order.authorization.address;
    storage_address = msg_verfy_sig_storage_order.order.storage_address;
    file_uri = msg_verfy_sig_storage_order.order.file_uri;
    content_unit_uri = msg_verfy_sig_storage_order.order.content_unit_uri;
    session_id = msg_verfy_sig_storage_order.order.session_id;
    seconds = msg_verfy_sig_storage_order.order.seconds;
    tp = chrono::system_clock::from_time_t(msg_verfy_sig_storage_order.order.time_point.tm);

    if (msg_verfy_sig_storage_order.order.seconds != SIGN_SECONDS)
        return false;

    auto now = chrono::system_clock::now();
    auto signed_time_point = chrono::system_clock::from_time_t(msg_verfy_sig_storage_order.order.time_point.tm);
    auto expiring_time_point = signed_time_point + chrono::seconds(msg_verfy_sig_storage_order.order.seconds);

    if (signed_time_point > now + chrono::seconds(NODES_TIME_SHIFT) ||
        expiring_time_point <= now - chrono::seconds(NODES_TIME_SHIFT))
        return false;

    bool correct = meshpp::verify_signature(
                       msg_verfy_sig_storage_order.authorization.address,
                       msg_verfy_sig_storage_order.order.to_string(),
                       msg_verfy_sig_storage_order.authorization.signature);

    if (false == correct)
        return false;

    return true;
}
}


