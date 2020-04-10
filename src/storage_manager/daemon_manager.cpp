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

using beltpp::packet;
using peer_id = beltpp::socket::peer_id;
using std::unordered_set;
using std::unordered_map;
using namespace BlockchainMessage;
using std::string;
namespace chrono = std::chrono;

using sf = beltpp::socket_family_t<&BlockchainMessage::message_list_load>;

static inline
beltpp::void_unique_ptr bm_get_putl()
{
    beltpp::message_loader_utility utl;
    BlockchainMessage::detail::extension_helper(utl);

    auto ptr_utl = beltpp::new_void_unique_ptr<beltpp::message_loader_utility>(std::move(utl));

    return ptr_utl;
}
static inline
beltpp::void_unique_ptr mm_get_putl()
{
    beltpp::message_loader_utility utl;
    ManagerMessage::detail::extension_helper(utl);

    auto ptr_utl = beltpp::new_void_unique_ptr<beltpp::message_loader_utility>(std::move(utl));

    return ptr_utl;
}

using TransactionLogLoader = sm_sync_context::TransactionLogLoader;
using LogIndexLoader = sm_sync_context::LogIndexLoader;

namespace detail
{
class sm_sync_context_detail
{
public:
    sm_sync_context_detail() = default;
    virtual ~sm_sync_context_detail() = default;

    virtual bool is_new_import() const = 0;
    virtual uint64_t& start_index() = 0;
    virtual uint64_t& head_block_index() = 0;
    virtual unordered_set<string> set_accounts() const = 0;
    virtual void save() = 0;
    virtual void commit() = 0;

    std::unordered_map<std::string, TransactionLogLoader> transactions;
    std::unordered_map<std::string, LogIndexLoader> index_transactions;
};

class sm_sync_context_new_import : public sm_sync_context_detail
{
public:
    sm_sync_context_new_import(manager& sm_server, string const& account)
        : sm_sync_context_detail()
        , m_sm_server(&sm_server)
        , m_start_index(0)
        , m_head_block_index(0)
        , m_account(account)
        , m_guard([this]()
                    {
                        for (auto& tr : transactions)
                            tr.second.discard();
                        for (auto& tr : index_transactions)
                            tr.second.discard();
                    })
    {}

    bool is_new_import() const override
    {
        return true;
    }
    uint64_t& start_index() override
    {
        return m_start_index;
    }
    uint64_t& head_block_index() override
    {
        return m_head_block_index;
    }
    unordered_set<string> set_accounts() const override
    {
        return unordered_set<string>{m_account};
    }
    void save() override
    {
        for (auto& tr : transactions)
            tr.second.save();
        for (auto& tr : index_transactions)
            tr.second.save();
    }
    void commit() override
    {
        m_guard.dismiss();

        for (auto& tr : transactions)
            tr.second.commit();
        for (auto& tr : index_transactions)
            tr.second.commit();
    }

    manager* m_sm_server;
    uint64_t m_start_index;
    uint64_t m_head_block_index;
    string m_account;
    beltpp::on_failure m_guard;
};

class sm_sync_context_existing : public sm_sync_context_detail
{
public:
    sm_sync_context_existing(manager& sm_server,
                             sm_daemon& sm_daemon,
                             unordered_set<string> const& set_accounts)
        : sm_sync_context_detail()
        , m_sm_daemon(&sm_daemon)
        , m_sm_server(&sm_server)
        , m_set_accounts(set_accounts)
        , m_guard([this]()
                    {
                        m_sm_daemon->log_index.discard();
                        m_sm_server->head_block_index.discard();
                        m_sm_server->blocks.discard();
                        m_sm_server->storages.discard();
                        m_sm_server->files.discard();

                        for (auto& tr : transactions)
                            tr.second.discard();
                        for (auto& tr : index_transactions)
                            tr.second.discard();
                    })
    {}

    bool is_new_import() const override
    {
        return false;
    }
    uint64_t& start_index() override
    {
        return m_sm_daemon->log_index->value;
    }
    uint64_t& head_block_index() override
    {
        return m_sm_server->head_block_index->value;
    }
    unordered_set<string> set_accounts() const override
    {
        return m_set_accounts;
    }
    void save() override
    {
        m_sm_server->head_block_index.save();
        m_sm_server->blocks.save();
        m_sm_server->storages.save();
        m_sm_server->files.save();
        m_sm_daemon->log_index.save();

        for (auto& tr : transactions)
            tr.second.save();
        for (auto& tr : index_transactions)
            tr.second.save();
    }
    void commit() override
    {
        m_guard.dismiss();

        m_sm_server->head_block_index.commit();
        m_sm_server->blocks.commit();
        m_sm_server->storages.commit();
        m_sm_server->files.commit();
        m_sm_daemon->log_index.commit();

        for (auto& tr : transactions)
            tr.second.commit();
        for (auto& tr : index_transactions)
            tr.second.commit();
    }

    sm_daemon* m_sm_daemon;
    manager* m_sm_server;

    unordered_set<string> m_set_accounts;
    beltpp::on_failure m_guard;
};
}

sm_sync_context::sm_sync_context(manager& sm_server, string const& account)
    : m_pimpl(new ::detail::sm_sync_context_new_import(sm_server, account))
{}
sm_sync_context::sm_sync_context(manager& sm_server,
                                 sm_daemon& sm_daemon,
                                 unordered_set<string> const& set_accounts)
    : m_pimpl(new ::detail::sm_sync_context_existing(sm_server, sm_daemon, set_accounts))
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

TransactionLogLoader& sm_sync_context::transactions(string const& account)
{
    if (0 == m_pimpl->set_accounts().count(account))
        throw std::logic_error("0 == m_pimpl->set_accounts().count(account)");

    auto tl_insert_res = m_pimpl->transactions.emplace(std::make_pair(account,
                                                                      sm_daemon::get_transaction_log(account)));

    return tl_insert_res.first->second;
}

LogIndexLoader& sm_sync_context::index_transactions(string const& account)
{
    if (0 == m_pimpl->set_accounts().count(account))
        throw std::logic_error("0 == m_pimpl->set_accounts().count(account)");

    auto idx_insert_res = m_pimpl->index_transactions.emplace(std::make_pair(account,
                                                                             sm_daemon::get_transaction_log_index(account)));

    return idx_insert_res.first->second;
}

sm_daemon::sm_daemon()
    : eh(beltpp::libsocket::construct_event_handler())
    , socket(beltpp::libsocket::getsocket<sf>(*eh))
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

void process_transaction(uint64_t block_index,
                         string const& str_account,
                         BlockchainMessage::TransactionLog const& transaction_log,
                         sm_sync_context& context,
                         LoggingType type)
{
    if (context.m_pimpl->set_accounts().count(str_account))
    {
        auto& tlogloader = context.transactions(str_account);
        auto& idxlogloader = context.index_transactions(str_account);

        if (LoggingType::apply == type)
            tlogloader.push_back(transaction_log);
        else
            tlogloader.pop_back();

        string str_block_index = std::to_string(block_index);

        if (LoggingType::apply == type)
        {
            size_t current_tx_index = tlogloader.size() - 1;
            ManagerMessage::NumberPair value;
            value.first = current_tx_index;
            value.second = 1;

            if (false == idxlogloader.insert(str_block_index, value))
            {
                auto& stored_value = idxlogloader.at(str_block_index);

                bool verify = stored_value.first + stored_value.second == current_tx_index;
                assert(verify);
                if (false == verify)
                    throw std::logic_error("cannot add to transaction index");

                ++stored_value.second;
            }
        }
        else
        {
            size_t current_tx_index = tlogloader.size();

            bool verify = idxlogloader.contains(str_block_index);
            assert(verify);
            if (false == verify)
                throw std::logic_error("cannot remove from transaction index");

            auto& stored_value = idxlogloader.at(str_block_index);

            verify = stored_value.first + stored_value.second == current_tx_index + 1;
            assert(verify);
            if (false == verify)
                throw std::logic_error("cannot remove from transaction index - check error");

            if (stored_value.second == 1)
                idxlogloader.erase(str_block_index);
            else
                --stored_value.second;
        }
    }
}

void process_storage_transactions(unordered_set<string> const& set_accounts,
                                  BlockchainMessage::TransactionLog const& transaction_log,
                                  manager& sm_server,
                                  LoggingType type)
{
    if (StorageUpdate::rtt == transaction_log.action.type())
    {
        StorageUpdate storage_update;
        transaction_log.action.get(storage_update);

        if (set_accounts.count(storage_update.storage_address))
        {
            bool is_stored = false;

            if ((UpdateType::store == storage_update.status &&
                LoggingType::apply == type) ||
                (UpdateType::remove == storage_update.status &&
                LoggingType::revert == type))
                    is_stored = true;

            if ((UpdateType::store == storage_update.status &&
                LoggingType::revert == type) ||
                (UpdateType::remove == storage_update.status &&
                LoggingType::apply == type))
                    is_stored = false;

            if (!sm_server.storages.contains(storage_update.storage_address))
            {
                ManagerMessage::StoragesResponseItem storage;
                storage.storage_address = storage_update.storage_address;
                storage.file_uris[storage_update.file_uri] = is_stored;

                sm_server.storages.insert(storage_update.storage_address, storage);
            }
            else
            {
                auto& storage = sm_server.storages.at(storage_update.storage_address);
                auto stored_file_uri = storage.file_uris.find(storage_update.file_uri);
                if (stored_file_uri == storage.file_uris.end())
                    storage.file_uris[storage_update.file_uri] = is_stored;
                else
                {
                    assert (stored_file_uri->second != is_stored);
                    if (stored_file_uri->second == is_stored)
                        throw std::logic_error("stored_file_uri->second == is_stored");

                    stored_file_uri->second = is_stored;
                }
            }
        }
    }
}

void process_statistics_transactions(BlockchainMessage::TransactionLog const& transaction_log,
                                     manager& sm_server,
                                     uint64_t block_index,
                                     LoggingType type)
{
    if (false == sm_server.m_str_pv_key.empty() &&
        ServiceStatistics::rtt == transaction_log.action.type())
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

TransactionLogLoader sm_daemon::get_transaction_log(string const& address)
{
    return
    TransactionLogLoader("tx",
                         meshpp::data_directory_path("accounts_log", address),
                         1000,
                         10,
                         bm_get_putl());
}
LogIndexLoader sm_daemon::get_transaction_log_index(string const& address)
{
    return
    LogIndexLoader("index_tx",
                   meshpp::data_directory_path("accounts_log", address),
                   1000,
                   mm_get_putl());
}

beltpp::packet sm_daemon::process_storage_update_request(ManagerMessage::StorageUpdateRequest const& update,
                                                              manager& sm_server)
{
    B_UNUSED(sm_server);

    if (peerid.empty())
        throw std::runtime_error("no daemon_rpc connection to work");

    beltpp::packet result;
    string transaction_hash;

    BlockchainMessage::StorageUpdate su;
    BlockchainMessage::from_string(update.status, su.status);
    su.file_uri = update.file_uri;
    su.storage_address = update.storage_address;

    BlockchainMessage::Transaction tx;
    tx.action = std::move(su);
    tx.fee = update.fee;
    tx.creation.tm = chrono::system_clock::to_time_t(chrono::system_clock::now());
    tx.expiry.tm =  chrono::system_clock::to_time_t(chrono::system_clock::now() + chrono::seconds(update.seconds_to_expire));

    transaction_hash = meshpp::hash(tx.to_string());

    BlockchainMessage::TransactionBroadcastRequest bc;
    bc.private_key = update.private_key;
    bc.transaction_details = tx;

    socket->send(peerid, beltpp::packet(std::move(bc)));

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
                case BlockchainMessage::TransactionDone::rtt:
                {
                    BlockchainMessage::TransactionDone done;
                    std::move(ref_packet).get(done);
                    result = std::move(done);
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

sm_sync_context sm_daemon::start_new_import(manager& sm_server, string const& account)
{
    return sm_sync_context(sm_server, account);
}

sm_sync_context sm_daemon::start_sync(manager& sm_server, unordered_set<string> const& set_accounts)
{
    return sm_sync_context(sm_server, *this, set_accounts);
}

beltpp::packet sm_daemon::send(ManagerMessage::Send const& send, manager& sm_server)
{
    B_UNUSED(sm_server);

    if (peerid.empty())
        throw std::runtime_error("no daemon_rpc connection to work");

    string transaction_hash;

    meshpp::private_key pv(send.private_key);
    //meshpp::public_key pb(send.to);

    BlockchainMessage::Transfer tf;
    tf.amount = send.amount;
    //publiqpp::coin(send.amount).to_Coin(tf.amount);
    tf.from = pv.get_public_key().to_string();
    tf.message = send.message;
    tf.to = send.to;

    BlockchainMessage::Transaction tx;
    tx.action = std::move(tf);
    tx.fee = send.fee;
    //publiqpp::coin(send.fee).to_Coin(tx.fee);
    tx.creation.tm = chrono::system_clock::to_time_t(chrono::system_clock::now());
    tx.expiry.tm =  chrono::system_clock::to_time_t(chrono::system_clock::now() + chrono::seconds(send.seconds_to_expire));

    Authority authorization;
    authorization.address = pv.get_public_key().to_string();
    authorization.signature = pv.sign(tx.to_string()).base58;

    BlockchainMessage::SignedTransaction stx;
    stx.transaction_details = std::move(tx);
    stx.authorizations.push_back(authorization);

    transaction_hash = meshpp::hash(stx.to_string());

    BlockchainMessage::Broadcast bc;
    bc.echoes = 2;
    bc.package = std::move(stx);

    socket->send(peerid, beltpp::packet(std::move(bc)));

    return wait_response(transaction_hash);
}

std::string time_now()
{
    std::time_t time_t_now = chrono::system_clock::to_time_t(chrono::system_clock::now());
    string str = beltpp::gm_time_t_to_lc_string(time_t_now);
    return str.substr(string("0000-00-00 ").length());
}

void sm_daemon::sync(manager& rpc_server, sm_sync_context& context)
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

                                        if (false == context.m_pimpl->is_new_import())
                                        {
                                            ManagerMessage::BlockInfo block_info;

                                            block_info.authority = block_log.authority;
                                            block_info.block_hash = block_log.block_hash;
                                            block_info.block_number = block_log.block_number;
                                            block_info.block_size = block_log.block_size;
                                            block_info.time_signed.tm = block_log.time_signed.tm;

                                            assert(block_log.block_number == block_index);
                                            rpc_server.blocks.push_back(block_info);
                                        }

                                        for (auto& transaction_log: block_log.transactions)
                                        {
                                            process_storage_transactions(context.m_pimpl->set_accounts(),
                                                                         transaction_log,
                                                                         rpc_server,
                                                                         LoggingType::apply);

                                            if (false == context.m_pimpl->is_new_import())
                                                process_statistics_transactions(transaction_log,
                                                                                rpc_server,
                                                                                block_index,
                                                                                LoggingType::apply);
                                        }
                                    }
                                    else if (action_type == TransactionLog::rtt)
                                    {
                                        //uint64_t block_index = context.m_pimpl->head_block_index() + 1;

                                        TransactionLog transaction_log;
                                        std::move(action_info.action).get(transaction_log);

                                        process_storage_transactions(context.m_pimpl->set_accounts(),
                                                                     transaction_log,
                                                                     rpc_server,
                                                                     LoggingType::apply);
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

                                            process_storage_transactions(context.m_pimpl->set_accounts(),
                                                                         transaction_log,
                                                                         rpc_server,
                                                                         LoggingType::revert);

                                            if (false == context.m_pimpl->is_new_import())
                                                process_statistics_transactions(transaction_log,
                                                                                rpc_server,
                                                                                block_index,
                                                                                LoggingType::revert);
                                        }

                                        if (false == context.m_pimpl->is_new_import())
                                        {
                                            rpc_server.blocks.pop_back();
                                        }
                                    }
                                    else if (action_type == TransactionLog::rtt)
                                    {
                                        TransactionLog transaction_log;
                                        std::move(action_info.action).get(transaction_log);

                                        //uint64_t block_index = context.m_pimpl->head_block_index() + 1;

                                        process_storage_transactions(context.m_pimpl->set_accounts(),
                                                                     transaction_log,
                                                                     rpc_server,
                                                                     LoggingType::revert);
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
