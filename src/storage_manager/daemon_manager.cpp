#include "daemon_manager.hpp"
#include "manager.hpp"
#include "utility.hpp"

#include <belt.pp/socket.hpp>
#include <belt.pp/scope_helper.hpp>

#include <mesh.pp/settings.hpp>
#include <mesh.pp/cryptoutility.hpp>
#include <publiq.pp/coin.hpp>

#include <unordered_set>
#include <unordered_map>
#include <exception>
#include <stdexcept>
#include <chrono>

#define LOGGING
#ifdef LOGGING
#include <iostream>
#endif

using std::string;
using std::unordered_set;
using std::unordered_map;

using beltpp::packet;
using peer_id = beltpp::socket::peer_id;

using namespace BlockchainMessage;

sm_daemon::sm_daemon(manager& server)
    : sm_server(server)
    , eh(beltpp::libsocket::construct_event_handler())
    , socket(beltpp::libsocket::getsocket<beltpp::socket_family_t<&BlockchainMessage::message_list_load>>(*eh))
    , peerid()
    , log_index(meshpp::data_file_path("log_index.txt"))
    , m_guard([this]()
                {
                    log_index.discard();
                    sm_server.files.discard();
                    sm_server.storages.discard();
                    sm_server.head_block_index.discard();
                })
{
    eh->add(*socket);
}

void sm_daemon::open(beltpp::ip_address const& connect_to_address)
{
    auto peerids = socket->open(connect_to_address);

    if (peerids.size() != 1)
        throw std::runtime_error(connect_to_address.to_string() + " is ambigous or unknown");

    bool keep_trying = true;
    while (keep_trying)
    {
        unordered_set<beltpp::event_item const*> wait_sockets;
        auto wait_result = eh->wait(wait_sockets);
        B_UNUSED(wait_sockets);

        if (wait_result & beltpp::event_handler::event)
        {
            peer_id _peerid;

            auto received_packets = socket->receive(_peerid);

            if (peerids.front() != _peerid)
                throw std::logic_error("logic error in open() - peerids.front() != peerid");

            for (auto& received_packet : received_packets)
            {
                packet& ref_packet = received_packet;

                switch (ref_packet.type())
                {
                case beltpp::stream_join::rtt:
                {
                    peerid = _peerid;
                    keep_trying = false;
                    break;
                }
                default:
                    throw std::runtime_error(connect_to_address.to_string() + " cannot open");
                }

                if (false == keep_trying)
                    break;
            }
        }
    }
}

void sm_daemon::close()
{
    if (peerid.empty())
        throw std::runtime_error("no daemon_rpc connection to close");

    socket->send(peerid, beltpp::packet(beltpp::stream_drop()));
}

void sm_daemon::save()
{
    log_index.save();
    sm_server.files.save();
    sm_server.storages.save();
    sm_server.head_block_index.save();
}

void sm_daemon::commit()
{
    m_guard.dismiss();

    log_index.commit();
    sm_server.files.commit();
    sm_server.storages.commit();
    sm_server.head_block_index.commit();
}

void process_unit_transactions(BlockchainMessage::TransactionLog const& transaction_log,
                               manager& sm_server,
                               LoggingType type)
{
    if (ContentUnit::rtt == transaction_log.action.type())
    {
        ContentUnit content_unit;
        transaction_log.action.get(content_unit);

        if (LoggingType::apply == type)
        {
            for (auto const& uri : content_unit.file_uris)
            {
                if (false == sm_server.files.contains(uri))
                {
                    ManagerMessage::FileInfo file_info;
                    file_info.uri = uri;
                    file_info.repl_count = 0;
                    file_info.last_report = 0;
                    file_info.channel_address = content_unit.channel_address;

                    sm_server.files.insert(uri, file_info);
                }
            }
        }
        else //if (LoggingType::revert == type)
        {
            // don't remove file info from storage
            // it can be used in another content unit as well
        }
    }
}

void process_storage_transactions(BlockchainMessage::TransactionLog const& transaction_log,
                                  manager& sm_server,
                                  LoggingType type)
{
    if (StorageUpdate::rtt == transaction_log.action.type())
    {
        StorageUpdate storage_update;
        transaction_log.action.get(storage_update);

        if (sm_server.files.contains(storage_update.file_uri))
        {
            ManagerMessage::FileInfo& file_info = sm_server.files.at(storage_update.file_uri);

            if ((UpdateType::store == storage_update.status && LoggingType::apply == type) ||
                (UpdateType::remove == storage_update.status && LoggingType::revert == type))
            {
                // store

                bool insert = true;

                for (auto it = file_info.all_storages.cbegin(); it != file_info.all_storages.cend() && insert; ++it)
                    insert = *it != storage_update.storage_address;

                if (insert)
                    file_info.all_storages.push_back(storage_update.storage_address);

                insert = sm_server.storages.contains(storage_update.storage_address);

                for (auto it = file_info.own_storages.cbegin(); it != file_info.own_storages.cend() && insert; ++it)
                    insert = *it != storage_update.storage_address;

                if (insert)
                    file_info.own_storages.push_back(storage_update.storage_address);
            }
            else
            {   // remove
                auto all_end = std::remove_if(file_info.all_storages.begin(), file_info.all_storages.end(),
                    [&storage_update](string const& storage_address)
                {
                    return storage_address == storage_update.storage_address;
                });
                file_info.all_storages.erase(all_end, file_info.all_storages.end());

                auto own_end = std::remove_if(file_info.own_storages.begin(), file_info.own_storages.end(),
                    [&storage_update](string const& storage_address)
                {
                    return storage_address == storage_update.storage_address;
                });
                file_info.own_storages.erase(own_end, file_info.own_storages.end());
            }
        }
    }
}

void process_statistics_transactions(BlockchainMessage::TransactionLog const& transaction_log,
                                     manager& sm_server,
                                     uint64_t block_index,
                                     LoggingType type)
{
    if (ServiceStatistics::rtt == transaction_log.action.type())
    {
        ServiceStatistics statistics;
        transaction_log.action.get(statistics);

        if (LoggingType::apply == type)
        {
            for (auto const& file_item : statistics.file_items)
            {
                // here take only channel provided statistocs
                if (false == file_item.unit_uri.empty())
                {
                    for (auto const& count_item : file_item.count_items)
                        sm_server.m_file_usage_map[block_index][file_item.file_uri] += count_item.count;

                    ManagerMessage::FileInfo& file_info = sm_server.files.at(file_item.file_uri);
                    file_info.last_report = block_index;
                }
            }
        }
        else //if (LoggingType::revert == type)
        {
            if (sm_server.m_file_usage_map.count(block_index))
                sm_server.m_file_usage_map.erase(block_index);
        }
    }
}

beltpp::packet sm_daemon::wait_response(string const& transaction_hash)
{
    beltpp::packet result;
    
    bool keep_trying = true;
    while (keep_trying)
    {
        unordered_set<beltpp::event_item const*> wait_sockets;
        auto wait_result = eh->wait(wait_sockets);
        B_UNUSED(wait_sockets);

        if (wait_result & beltpp::event_handler::event)
        {
            peer_id _peerid;

            auto received_packets = socket->receive(_peerid);

            for (auto& received_packet : received_packets)
            {
                packet& ref_packet = received_packet;

                switch (ref_packet.type())
                {
                case BlockchainMessage::Done::rtt:
                {
                    ManagerMessage::StringValue response;
                    response.value = transaction_hash;
                    result = std::move(response);
                    peerid = _peerid;
                    keep_trying = false;
                    break;
                }
                case beltpp::stream_drop::rtt:
                {
                    ManagerMessage::Failed response;
                    response.message = "server disconnected";
                    response.reason = std::move(ref_packet);
                    result = std::move(response);
                    keep_trying = false;
                    break;
                }
                default:
                {
                    ManagerMessage::Failed response;
                    response.message = "error";
                    response.reason = std::move(ref_packet);
                    result = std::move(response);
                    keep_trying = false;
                }
                if (false == keep_trying)
                    break;
                }
            }
        }
    }

    return result;
}

std::string time_now()
{
    std::time_t time_t_now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    string str = beltpp::gm_time_t_to_lc_string(time_t_now);
    return str.substr(string("0000-00-00 ").length());
}

void sm_daemon::sync()
{
    if (peerid.empty())
        throw std::runtime_error("no daemon_rpc connection to work");

    while (true)
    {
        size_t const max_count = 10000;
        LoggedTransactionsRequest req;
        req.max_count = max_count;
        req.start_index = log_index->value;

        socket->send(peerid, beltpp::packet(req));

        size_t count = 0;
        
#ifdef LOGGING
        std::cout << std::endl << std::endl << time_now() << "  Request from index -> " + std::to_string(log_index->value);
#endif
        
        while (true)
        {
            unordered_set<beltpp::event_item const*> wait_sockets;
            auto wait_result = eh->wait(wait_sockets);
            B_UNUSED(wait_sockets);

            if (wait_result & beltpp::event_handler::event)
            {
                peer_id _peerid;

                auto received_packets = socket->receive(_peerid);

                for (auto& received_packet : received_packets)
                {
                    packet& ref_packet = received_packet;

                    switch (ref_packet.type())
                    {
                        case LoggedTransactions::rtt:
                        {
                            LoggedTransactions msg;
                            std::move(ref_packet).get(msg);

                            for (auto& action_info : msg.actions)
                            {
                                ++count;

                                log_index->value = action_info.index + 1;

                                auto action_type = action_info.action.type();

                                if (action_info.logging_type == LoggingType::apply)
                                {
                                    if (action_type == BlockLog::rtt)
                                    {
                                        ++sm_server.head_block_index->value;

                                        BlockLog block_log;
                                        std::move(action_info.action).get(block_log);

                                        uint64_t block_index = sm_server.head_block_index->value;

                                        count += block_log.rewards.size() +
                                                 block_log.transactions.size() +
                                                 block_log.unit_uri_impacts.size() + 
                                                 block_log.applied_sponsor_items.size();
                                    
                                        for (auto& transaction_log: block_log.transactions)
                                        {
                                            process_unit_transactions(transaction_log,
                                                                      sm_server,
                                                                      LoggingType::apply);

                                            process_storage_transactions(transaction_log,
                                                                         sm_server,
                                                                         LoggingType::apply);

                                            process_statistics_transactions(transaction_log,
                                                                            sm_server,
                                                                            block_index,
                                                                            LoggingType::apply);
                                        }
                                    }
                                }
                                else// if (action_info.logging_type == LoggingType::revert)
                                {
                                    if (action_type == BlockLog::rtt)
                                    {
                                        uint64_t block_index = sm_server.head_block_index->value;

                                        --sm_server.head_block_index->value;

                                        BlockLog block_log;
                                        std::move(action_info.action).get(block_log);

                                        count += block_log.rewards.size() +
                                                 block_log.transactions.size() +
                                                 block_log.unit_uri_impacts.size() +
                                                 block_log.applied_sponsor_items.size();

                                        for (auto log_it = block_log.transactions.crbegin(); log_it != block_log.transactions.crend(); ++log_it)
                                        {
                                            auto& transaction_log = *log_it;

                                            process_unit_transactions(transaction_log,
                                                                      sm_server,
                                                                      LoggingType::revert);

                                            process_storage_transactions(transaction_log,
                                                                         sm_server,
                                                                         LoggingType::revert);

                                            process_statistics_transactions(transaction_log,
                                                                            sm_server,
                                                                            block_index,
                                                                            LoggingType::revert);
                                        }
                                    }
                                }
                            }//  for (auto& action_info : msg.actions)

                            break;  //  breaks switch case
                        }
                        default:
                            throw std::runtime_error(std::to_string(ref_packet.type()) + " - sync cannot handle");
                    }
                }//for (auto& received_packet : received_packets)

                if (false == received_packets.empty())
                    break;  //  breaks while() that calls receive(), may send another request
            }
        }//  while (true) and call eh->wait(wait_sockets)

        if (count < max_count)
            break; //   will not send any more requests
    }//  while (true) and socket->send(peerid, beltpp::packet(req));
}
