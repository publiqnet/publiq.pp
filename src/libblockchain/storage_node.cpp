#include "storage_node.hpp"
#include "common.hpp"
#include "storage_node_internals.hpp"

#include "open_container_packet.hpp"

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
                vector<packet*> composition;
                open_container_packet<TaskRequest> any_task;

                if (false == any_task.open(received_packet, composition, m_pimpl->m_pv_key.get_public_key()))
                {
                    composition.clear();
                    composition.push_back(&received_packet);
                }

                packet& ref_packet = *composition.back();

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
                case StorageFile::rtt:
                {
                    TaskRequest* p_task_request = nullptr;
                    StorageFile* p_storage_file = nullptr;

                    any_task.items[0]->get(p_task_request);
                    ref_packet.get(p_storage_file);

                    assert(p_task_request);
                    assert(p_storage_file);

                    TaskRequest& task_request = *p_task_request;
                    StorageFile& storage_file = *p_storage_file;

                    StorageFileAddress file_address;
                    file_address.uri = m_pimpl->m_storage.put(std::move(storage_file));

                    TaskResponse task_response;
                    task_response.task_id = task_request.task_id;
                    task_response.package = file_address;

                    psk->send(peerid, task_response);

                    break;
                }
                case StorageFileRequest::rtt:
                {
                    StorageFileRequest file_info;
                    std::move(ref_packet).get(file_info);
                    
                    StorageFile file;
                    if (m_pimpl->m_storage.get(file_info.uri, file))
                    {
                        psk->send(peerid, std::move(file));
                    }
                    else
                    {
                        FileNotFound error;
                        error.uri = file_info.uri;
                        psk->send(peerid, std::move(error));
                    }

                    break;
                }
                case ServiceStatistics::rtt:
                {/*
                    TaskRequest* p_task_request = nullptr;
                    any_task.items[0]->get(p_task_request);
                   
                    assert(p_task_request);
                    
                    TaskRequest& task_request = *p_task_request;
                    
                    ServiceStatistics stat_info;
                    
                    m_pimpl->m_stat_counter.get_stat_info(stat_info);
                    stat_info.reporter_address = m_pimpl->m_pv_key.get_public_key().to_string();

                    TaskResponse task_response;
                    task_response.package = stat_info;
                    task_response.task_id = task_request.task_id;
                    
                    psk->send(peerid, task_response);

                    m_pimpl->m_stat_counter.init();
                   */ 
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
                    psk->send(peerid, std::move(msg_pong));
                    break;
                }
                default:
                {
                    m_pimpl->writeln_node("slave don't know how to handle: " + std::to_string(ref_packet.type()) +
                                          " from " + detail::peer_short_names(peerid));

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
                case StorageFile::rtt:
                {
                    StorageFile storage_file;
                    std::move(request).get(storage_file);
                    StorageFileAddress file_address;
                    file_address.uri = m_pimpl->m_storage.put(std::move(storage_file));

                    response.set(std::move(file_address));
                    m_pimpl->m_master_node->wake();
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


