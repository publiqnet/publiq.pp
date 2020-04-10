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
#include <chrono>

//#define LOGGING
#ifdef LOGGING
#include <iostream>
#endif

using std::string;
using std::unordered_set;
using std::unordered_map;

using beltpp::packet;
using peer_id = beltpp::socket::peer_id;

using namespace BlockchainMessage;

namespace detail
{
class sm_sync_context_internals
{
public:
    sm_sync_context_internals() = default;
    virtual ~sm_sync_context_internals() = default;

    virtual uint64_t& start_index() = 0;
    virtual uint64_t& head_block_index() = 0;
    virtual void save() = 0;
    virtual void commit() = 0;
};

class sm_sync_context_import : public sm_sync_context_internals
{
public:
    sm_sync_context_import(manager& sm_server, string const& address)
        : sm_sync_context_internals()
        , m_address(address)
        , m_sm_server(&sm_server)
        , m_start_index(0)
        , m_head_block_index(0)
    {}

    uint64_t& start_index() override
    {
        return m_start_index;
    }

    uint64_t& head_block_index() override
    {
        return m_head_block_index;
    }

    void save() override
    {
        // nothing to save
    }

    void commit() override
    {
        // nothing to commit
    }

    string m_address;
    manager* m_sm_server;
    uint64_t m_start_index;
    uint64_t m_head_block_index;
};

class sm_sync_context_existing : public sm_sync_context_internals
{
public:
    sm_sync_context_existing(manager& sm_server, sm_daemon& sm_daemon)
        : sm_sync_context_internals()
        , m_sm_server(&sm_server)
        , m_sm_daemon(&sm_daemon)
        , m_guard([this]()
                    {
                        m_sm_server->files.discard();
                        m_sm_server->storages.discard();
                        m_sm_daemon->log_index.discard();
                        m_sm_server->head_block_index.discard();
                    })
    {}

    uint64_t& start_index() override
    {
        return m_sm_daemon->log_index->value;
    }

    uint64_t& head_block_index() override
    {
        return m_sm_server->head_block_index->value;
    }

    void save() override
    {
        m_sm_server->files.save();
        m_sm_server->storages.save();
        m_sm_daemon->log_index.save();
        m_sm_server->head_block_index.save();
    }

    void commit() override
    {
        m_guard.dismiss();

        m_sm_server->files.commit();
        m_sm_server->storages.commit();
        m_sm_daemon->log_index.commit();
        m_sm_server->head_block_index.commit();
    }

    manager* m_sm_server;
    sm_daemon* m_sm_daemon;
    beltpp::on_failure m_guard;
};
}

sm_sync_context::sm_sync_context(manager& sm_server, string const& address)
    : m_pimpl(new ::detail::sm_sync_context_import(sm_server, address))
{}

sm_sync_context::sm_sync_context(manager& sm_server, sm_daemon& sm_daemon)
    : m_pimpl(new ::detail::sm_sync_context_existing(sm_server, sm_daemon))
{}

sm_sync_context::sm_sync_context(sm_sync_context&&) = default;

sm_sync_context::~sm_sync_context() = default;

uint64_t sm_sync_context::start_index() const
{
    return m_pimpl->start_index();
}

void sm_sync_context::save()
{
    return m_pimpl->save();
}

void sm_sync_context::commit()
{
    return m_pimpl->commit();
}

sm_daemon::sm_daemon()
    : eh(beltpp::libsocket::construct_event_handler())
    , socket(beltpp::libsocket::getsocket<beltpp::socket_family_t<&BlockchainMessage::message_list_load>>(*eh))
    , peerid()
    , log_index(meshpp::data_file_path("log_index.txt"))
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
                    file_info.rep_count = 0;
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
                auto& storages = file_info.all_storages;

                for (auto it = storages.cbegin(); it != storages.cend() && insert; ++it)
                    insert = *it != storage_update.storage_address;

                if (insert)
                    file_info.all_storages.push_back(storage_update.storage_address);

                insert = sm_server.storages.contains(storage_update.storage_address);
                storages = file_info.own_storages;

                for (auto it = storages.cbegin(); it != storages.cend() && insert; ++it)
                    insert = *it != storage_update.storage_address;

                if (insert)
                    file_info.all_storages.push_back(storage_update.storage_address);
            }
            else
            {   // remove
                auto& storages = file_info.all_storages;

                for (auto it = storages.begin(); it != storages.end(); ++it)
                    if (*it == storage_update.storage_address)
                        storages.erase(it);

                storages = file_info.own_storages;

                for (auto it = storages.begin(); it != storages.end(); ++it)
                    if (*it == storage_update.storage_address)
                        storages.erase(it);
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
                        sm_server.file_usage_map[block_index][file_item.file_uri] += count_item.count;
                }
            }
        }
        else //if (LoggingType::revert == type)
        {
            if (sm_server.file_usage_map.count(block_index))
                sm_server.file_usage_map.erase(block_index);
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

sm_sync_context sm_daemon::start_sync(manager& sm_server)
{
    return sm_sync_context(sm_server, *this);
}

sm_sync_context sm_daemon::start_import(manager& sm_server, string const& address)
{
    return sm_sync_context(sm_server, address);
}

//std::string time_now()
//{
//    std::time_t time_t_now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
//    string str = beltpp::gm_time_t_to_lc_string(time_t_now);
//    return str.substr(string("0000-00-00 ").length());
//}

void sm_daemon::sync(manager& sm_server, sm_sync_context& context)
{
    if (peerid.empty())
        throw std::runtime_error("no daemon_rpc connection to work");

    while (true)
    {
        size_t const max_count = 10000;
        LoggedTransactionsRequest req;
        req.max_count = max_count;
        req.start_index = context.m_pimpl->start_index();

        socket->send(peerid, beltpp::packet(req));

        size_t count = 0;

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

                                bool dont_increment_head_block_index = false;
                                if (context.m_pimpl->start_index() == 0)
                                    dont_increment_head_block_index = true;

                                context.m_pimpl->start_index() = action_info.index + 1;

                                auto action_type = action_info.action.type();

                                if (action_info.logging_type == LoggingType::apply)
                                {
                                    if (action_type == BlockLog::rtt)
                                    {
                                        if (false == dont_increment_head_block_index)
                                            ++context.m_pimpl->head_block_index();

                                        BlockLog block_log;
                                        std::move(action_info.action).get(block_log);

                                        uint64_t block_index = context.m_pimpl->head_block_index();

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
                                        uint64_t block_index = context.m_pimpl->head_block_index();

                                        --context.m_pimpl->head_block_index();

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
