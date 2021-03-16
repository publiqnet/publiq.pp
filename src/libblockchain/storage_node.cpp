#include "storage_node.hpp"
#include "common.hpp"
#include "storage_node_internals.hpp"
#include "types.hpp"
#include "message.tmpl.hpp"
#include "open_container_packet.hpp"

#include <publiq.pp/storage_utility_rpc.hpp>

#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <exception>
#include <stdexcept>
#include <thread>

using namespace BlockchainMessage;

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
storage_node::storage_node(config& ref_config,
                           boost::filesystem::path const& fs_storage,
                           beltpp::ilog* plogger_storage_node,
                           beltpp::direct_channel& channel)
    : m_pimpl(new detail::storage_node_internals(ref_config,
                                                 fs_storage,
                                                 plogger_storage_node,
                                                 channel))
{}

storage_node::storage_node(storage_node&&) noexcept = default;

storage_node::~storage_node() = default;

void storage_node::wake()
{
    m_pimpl->m_ptr_eh->wake();
}

void storage_node::run(bool& stop)
{
    stop = false;

    unordered_set<beltpp::event_item const*> wait_sockets;

    m_pimpl->m_event_queue.next(*m_pimpl->m_ptr_eh,
                                m_pimpl->m_ptr_rpc_socket.get(),
                                nullptr,
                                m_pimpl->m_ptr_direct_stream.get());

    if (m_pimpl->m_event_queue.is_timer())
    {
        m_pimpl->m_ptr_rpc_socket->timer_action();
    }
    else if (m_pimpl->m_event_queue.is_message() &&
             m_pimpl->m_event_queue.message_source() != m_pimpl->m_ptr_direct_stream.get())
    {
        auto peerid = m_pimpl->m_event_queue.message_peerid();
        auto& ref_packet = m_pimpl->m_event_queue.message();

        beltpp::stream* psk = m_pimpl->m_ptr_rpc_socket.get();

        try
        {
            switch (ref_packet.type())
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
                std::move(ref_packet).get(msg);
                m_pimpl->writeln_node("slave has protocol error: " + detail::peer_short_names(peerid));
                m_pimpl->writeln_node(msg.buffer);
                psk->send(peerid, beltpp::packet(beltpp::stream_drop()));

                break;
            }
            case beltpp::socket_open_refused::rtt:
            {
                beltpp::socket_open_refused msg;
                std::move(ref_packet).get(msg);
                m_pimpl->writeln_node_warning(msg.reason + ", " + peerid);

                break;
            }
            case beltpp::socket_open_error::rtt:
            {
                beltpp::socket_open_error msg;
                std::move(ref_packet).get(msg);
                m_pimpl->writeln_node_warning(msg.reason + ", " + peerid);

                break;
            }
            case StorageFileRequest::rtt:
            {
                StorageFileRequest file_info;
                std::move(ref_packet).get(file_info);

                string file_uri;

                if (m_pimpl->pconfig->get_node_type() == NodeType::storage)
                {
                    string channel_address;
                    string storage_address;
                    string content_unit_uri;
                    string session_id;
                    uint64_t seconds;
                    system_clock::time_point tp;

                    if (false == storage_utility::rpc::verify_storage_order(file_info.storage_order_token,
                                                                            channel_address,
                                                                            storage_address,
                                                                            file_uri,
                                                                            content_unit_uri,
                                                                            session_id,
                                                                            seconds,
                                                                            tp) ||
                        storage_address != m_pimpl->front_public_key().to_string() ||
                        0 == m_pimpl->m_verified_channels.count(channel_address))
                        file_uri.clear();
                }
                else
                {
                    file_uri = file_info.uri;
                }

                StorageFile file;
                if (false == file_uri.empty() &&
                    m_pimpl->m_storage.get(file_uri, file))
                {
                    psk->send(peerid, beltpp::packet(std::move(file)));

                    if (m_pimpl->pconfig->get_node_type() == NodeType::storage)
                    {
                        Served msg;
                        msg.storage_order_token = file_info.storage_order_token;

                        StorageTypes::ContainerMessage msg_response;
                        msg_response.package.set(msg);
                        m_pimpl->m_ptr_direct_stream->send(node_peerid, packet(std::move(msg_response)));
                    }
                }
                else
                {
                    UriError error;
                    error.uri = file_uri;
                    error.uri_problem_type = UriProblemType::missing;
                    psk->send(peerid, beltpp::packet(std::move(error)));
                }

                break;
            }
            case StorageFileDetails::rtt:
            {
                StorageFileDetails details_request;
                std::move(ref_packet).get(details_request);

                StorageFile file;
                if (m_pimpl->m_storage.get(details_request.uri, file))
                {
                    StorageFileDetailsResponse details_response;
                    details_response.uri = details_request.uri;
                    details_response.size = file.data.length();
                    details_response.mime_type = file.mime_type;

                    psk->send(peerid, beltpp::packet(std::move(details_response)));
                }
                else
                {
                    UriError error;
                    error.uri = details_request.uri;
                    error.uri_problem_type = UriProblemType::missing;
                    psk->send(peerid, beltpp::packet(std::move(error)));
                }

                break;
            }
            case Ping::rtt:
            {
                Ping msg;
                std::move(ref_packet).get(msg);

                auto pv_key = m_pimpl->front_private_key();
                auto pb_key = m_pimpl->front_public_key();

                if (msg.address)
                {
                    bool skip_next = false;

                    for (auto const& pbkey_item : m_pimpl->pconfig->public_keys())
                    {
                        if (pbkey_item.to_string() == *msg.address)
                        {
                            pb_key = pbkey_item;
                            skip_next = true;
                            break;
                        }
                    }

                    if (false == skip_next)
                    for (auto const& key_item : m_pimpl->pconfig->keys())
                    {
                        if (key_item.get_public_key().to_string() == *msg.address)
                        {
                            pv_key = key_item;
                            pb_key = pv_key.get_public_key();
                            break;
                        }
                    }
                }

                Pong msg_pong;
                msg_pong.node_address = pb_key.to_string();
                msg_pong.stamp.tm = system_clock::to_time_t(system_clock::now());
                string message_pong = msg_pong.node_address + ::beltpp::gm_time_t_to_gm_string(msg_pong.stamp.tm);
                auto signed_message = pv_key.sign(message_pong);

                msg_pong.signature = std::move(signed_message.base58);
                psk->send(peerid, beltpp::packet(std::move(msg_pong)));
                break;
            }
            case SyncRequest::rtt:
            {
                if (nullptr == m_pimpl->m_sync_response ||
                    0 == m_pimpl->m_event_queue.count_rescheduled())
                {
                    if (0 == m_pimpl->m_event_queue.count_rescheduled())
                    {
                        // request to master node
                        StorageTypes::ContainerMessage msg_request;
                        msg_request.package.set(SyncRequest());
                        m_pimpl->m_ptr_direct_stream->send(node_peerid, packet(std::move(msg_request)));
                    }

                    // reshedule request
                    m_pimpl->m_event_queue.reschedule();
                }
                else
                {
                    SyncResponse response(std::move(*m_pimpl->m_sync_response));
                    m_pimpl->m_sync_response.reset();
                    psk->send(peerid, beltpp::packet(std::move(response)));
                }

                break;
            }
            default:
            {
                m_pimpl->writeln_node("slave can't handle: " + std::to_string(ref_packet.type()) +
                                      ". peer: " + peerid);

                psk->send(peerid, beltpp::packet(beltpp::stream_drop()));
                break;
            }
            }   // switch ref_packet.type()
        }
        catch (std::exception const& e)
        {
            RemoteError msg;
            msg.message = e.what();
            psk->send(peerid, beltpp::packet(std::move(msg)));
            throw;
        }
        catch (...)
        {
            RemoteError msg;
            msg.message = "unknown exception";
            psk->send(peerid, beltpp::packet(std::move(msg)));
            throw;
        }
    }
    else if (m_pimpl->m_event_queue.is_message() &&
             m_pimpl->m_event_queue.message_source() == m_pimpl->m_ptr_direct_stream.get())
    {
        auto peerid = m_pimpl->m_event_queue.message_peerid();
        auto& ref_packet = m_pimpl->m_event_queue.message();

        auto& stream = *m_pimpl->m_ptr_direct_stream;
        
        try
        {
            switch (ref_packet.type())
            {
            case StorageTypes::StorageFile::rtt:
            {
                StorageTypes::StorageFile storage_file_ex;
                std::move(ref_packet).get(storage_file_ex);

                assert(storage_file_ex.storage_file.type() == BlockchainMessage::StorageFile::rtt);
                if (storage_file_ex.storage_file.type() != BlockchainMessage::StorageFile::rtt)
                    throw std::logic_error("storage_file.storage_file.type() != BlockchainMessage::StorageFile::rtt");

                StorageFile storage_file;
                std::move(storage_file_ex.storage_file).get(storage_file);

                string uri;
                if (m_pimpl->m_storage.put(std::move(storage_file), uri))
                {
                    StorageFileAddress file_address;
                    file_address.uri = uri;

                    StorageTypes::ContainerMessage msg_response;
                    msg_response.package.set(std::move(file_address));
                    stream.send(peerid, packet(std::move(msg_response)));
                }
                else
                {
                    UriError msg;
                    msg.uri = uri;
                    msg.uri_problem_type = UriProblemType::duplicate;

                    StorageTypes::ContainerMessage msg_response;
                    msg_response.package.set(std::move(msg));
                    stream.send(peerid, packet(std::move(msg_response)));
                }
                
                break;
            }
            case StorageTypes::StorageFileDelete::rtt:
            {
                StorageTypes::StorageFileDelete storage_file_delete_ex;
                std::move(ref_packet).get(storage_file_delete_ex);

                assert(storage_file_delete_ex.storage_file_delete.type() == BlockchainMessage::StorageFileDelete::rtt);
                if (storage_file_delete_ex.storage_file_delete.type() != BlockchainMessage::StorageFileDelete::rtt)
                    throw std::logic_error("storage_file_delete_ex.storage_file_delete.type() != BlockchainMessage::StorageFileDelete::rtt");

                StorageFileDelete storage_file_delete;
                std::move(storage_file_delete_ex.storage_file_delete).get(storage_file_delete);

                if (m_pimpl->m_storage.remove(storage_file_delete.uri))
                {
                    StorageTypes::ContainerMessage msg_response;
                    msg_response.package.set(Done());
                    stream.send(peerid, packet(std::move(msg_response)));
                }
                else
                {
                    UriError msg;
                    msg.uri = storage_file_delete.uri;
                    msg.uri_problem_type = UriProblemType::missing;

                    StorageTypes::ContainerMessage msg_response;
                    msg_response.package.set(msg);
                    stream.send(peerid, packet(std::move(msg_response)));
                }

                break;
            }
            case StorageTypes::SetVerifiedChannels::rtt:
            {
                StorageTypes::SetVerifiedChannels channels;
                std::move(ref_packet).get(channels);

                m_pimpl->m_verified_channels.clear();
                for (auto const& channel_address : channels.channel_addresses)
                    m_pimpl->m_verified_channels.insert(channel_address);

                StorageTypes::ContainerMessage msg_response;
                msg_response.package.set(Done());
                stream.send(peerid, packet(std::move(msg_response)));
                break;
            }
            case StorageTypes::FileUrisRequest::rtt:
            {
                FileUris msg;

                auto set_file_uris = m_pimpl->m_storage.get_file_uris();
                msg.file_uris.reserve(set_file_uris.size());
                for (auto& file_uri : set_file_uris)
                    msg.file_uris.push_back(std::move(file_uri));

                StorageTypes::ContainerMessage msg_response;
                msg_response.package.set(msg);
                stream.send(peerid, packet(std::move(msg_response)));
                break;
            }
            case StorageTypes::ContainerMessage::rtt:
            {
                StorageTypes::ContainerMessage* pcontainer;
                ref_packet.get(pcontainer);

                StorageTypes::ContainerMessage& msg_container = *pcontainer;

                if (msg_container.package.type() == SyncResponse::rtt)
                {
                    if (m_pimpl->m_sync_response)
                        m_pimpl->m_event_queue.reschedule();
                    else
                    {
                        SyncResponse response;
                        std::move(msg_container.package).get(response);

                        m_pimpl->m_sync_response.reset(new SyncResponse(response));
                    }
                }

                break;
            }
            }
        }
        catch (std::exception const& e)
        {
            RemoteError msg;
            msg.message = e.what();

            StorageTypes::ContainerMessage msg_response;
            msg_response.package.set(msg);
            stream.send(peerid, packet(std::move(msg_response)));
            throw;
        }
        catch (...)
        {
            RemoteError msg;
            msg.message = "unknown error";

            StorageTypes::ContainerMessage msg_response;
            msg_response.package.set(msg);
            stream.send(peerid, packet(std::move(msg_response)));
            throw;
        }
    }
}

}


