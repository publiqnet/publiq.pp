#include "storage_node.hpp"
#include "common.hpp"
#include "storage_node_internals.hpp"
#include "types.hpp"
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
storage_node::storage_node(node& master_node,
                           ip_address const& rpc_bind_to_address,
                           boost::filesystem::path const& fs_storage,
                           meshpp::private_key const& pv_key,
                           beltpp::ilog* plogger_storage_node)
    : m_pimpl(new detail::storage_node_internals(master_node,
                                                 rpc_bind_to_address,
                                                 fs_storage,
                                                 pv_key,
                                                 plogger_storage_node))
{
    master_node.set_slave_node(*this);
}

storage_node::storage_node(storage_node&&) noexcept = default;

storage_node::~storage_node() = default;

void storage_node::wake()
{
    m_pimpl->m_ptr_eh->wake();
}

bool storage_node::run()
{
    bool code = true;

    unordered_set<beltpp::ievent_item const*> wait_sockets;

    auto wait_result = m_pimpl->m_ptr_eh->wait(wait_sockets);

    if (wait_result & beltpp::event_handler::event)
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
                    beltpp::isocket_protocol_error msg;
                    ref_packet.get(msg);
                    m_pimpl->writeln_node("slave has protocol error: " + detail::peer_short_names(peerid));
                    m_pimpl->writeln_node(msg.buffer);

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
                case StorageFileRequest::rtt:
                {
                    StorageFileRequest file_info;
                    std::move(ref_packet).get(file_info);

                    string file_uri;

                    if (m_pimpl->m_node_type == NodeType::storage)
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
                            storage_address != m_pimpl->m_pv_key.get_public_key().to_string() ||
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

                        if (m_pimpl->m_node_type == NodeType::storage)
                        {
                            std::lock_guard<std::mutex> lock(m_pimpl->m_messages_mutex);
                            Served msg;
                            msg.storage_order_token = file_info.storage_order_token;
                            m_pimpl->m_messages.push_back(std::make_pair(beltpp::packet(), packet(std::move(msg))));
                            m_pimpl->m_master_node->wake();
                        }
                    }
                    else
                    {
                        UriError error;
                        error.uri = file_info.uri;
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
                    Pong msg_pong;
                    msg_pong.node_address = m_pimpl->m_pv_key.get_public_key().to_string();
                    msg_pong.stamp.tm = system_clock::to_time_t(system_clock::now());
                    string message_pong = msg_pong.node_address + ::beltpp::gm_time_t_to_gm_string(msg_pong.stamp.tm);
                    auto signed_message = m_pimpl->m_pv_key.sign(message_pong);

                    msg_pong.signature = std::move(signed_message.base58);
                    psk->send(peerid, beltpp::packet(std::move(msg_pong)));
                    break;
                }
                default:
                {
                    m_pimpl->writeln_node("slave can't handle: " + std::to_string(ref_packet.type()) +
                                          ". peer: " + peerid);

                    psk->send(peerid, beltpp::packet(beltpp::isocket_drop()));
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
            }   // for (auto& received_packet : received_packets)
        }   // for (auto& pevent_item : wait_sockets)
    }

    if (wait_result & beltpp::event_handler::timer_out)
    {
        m_pimpl->m_ptr_rpc_socket->timer_action();
    }

    if (wait_result & beltpp::event_handler::on_demand)
    {
        std::lock_guard<std::mutex> lock(m_pimpl->m_messages_mutex);
        auto& messages = m_pimpl->m_messages;
        for (auto& item : messages)
        {
        auto& request = item.first;
        auto& response = item.second;
        try
        {
            if (response.empty())
            {
                switch (request.type())
                {
                case StorageTypes::StorageFile::rtt:
                {
                    StorageTypes::StorageFile storage_file_ex;
                    std::move(request).get(storage_file_ex);

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
                        response.set(std::move(file_address));
                    }
                    else
                    {
                        UriError msg;
                        msg.uri = uri;
                        msg.uri_problem_type = UriProblemType::duplicate;
                        response.set(std::move(msg));
                    }

                    m_pimpl->m_master_node->wake();
                    break;
                }
                case StorageTypes::StorageFileDelete::rtt:
                {
                    StorageTypes::StorageFileDelete storage_file_delete_ex;
                    std::move(request).get(storage_file_delete_ex);

                    assert(storage_file_delete_ex.storage_file_delete.type() == BlockchainMessage::StorageFileDelete::rtt);
                    if (storage_file_delete_ex.storage_file_delete.type() != BlockchainMessage::StorageFileDelete::rtt)
                        throw std::logic_error("storage_file_delete_ex.storage_file_delete.type() != BlockchainMessage::StorageFileDelete::rtt");

                    StorageFileDelete storage_file_delete;
                    std::move(storage_file_delete_ex.storage_file_delete).get(storage_file_delete);

                    if (m_pimpl->m_storage.remove(storage_file_delete.uri))
                    {
                        response.set(Done());
                    }
                    else
                    {
                        UriError msg;
                        msg.uri = storage_file_delete.uri;
                        msg.uri_problem_type = UriProblemType::missing;
                        response.set(std::move(msg));
                    }

                    m_pimpl->m_master_node->wake();
                    break;
                }
                case StorageTypes::SetVerifiedChannels::rtt:
                {
                    StorageTypes::SetVerifiedChannels channels;
                    std::move(request).get(channels);

                    m_pimpl->m_verified_channels.clear();
                    for (auto const& channel_address : channels.channel_addresses)
                        m_pimpl->m_verified_channels.insert(channel_address);
                    break;
                }
                }
            }
        }
        catch (std::exception const& e)
        {
            RemoteError msg;
            msg.message = e.what();
            response.set(std::move(msg));
            m_pimpl->m_master_node->wake();
            throw;
        }
        catch (...)
        {
            RemoteError msg;
            msg.message = "unknown error";
            response.set(std::move(msg));
            m_pimpl->m_master_node->wake();
            throw;
        }
        }
    }

    return code;
}

beltpp::isocket::packets storage_node::receive()
{
    std::lock_guard<std::mutex> lock(m_pimpl->m_messages_mutex);
    beltpp::isocket::packets result;

    auto& messages = m_pimpl->m_messages;
    while (false == messages.empty() &&
           false == messages.front().second.empty())
    {
        result.push_back(std::move(messages.front().second));
        messages.pop_front();
    }

    return result;
}

void storage_node::send(beltpp::packet&& pack)
{
    std::lock_guard<std::mutex> lock(m_pimpl->m_messages_mutex);
    m_pimpl->m_messages.push_back(std::make_pair(std::move(pack), beltpp::packet()));
}

}


