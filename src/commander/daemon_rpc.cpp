#include "daemon_rpc.hpp"
#include "rpc.hpp"

#include <belt.pp/socket.hpp>
#include <belt.pp/scope_helper.hpp>

#include <mesh.pp/settings.hpp>
#include <mesh.pp/cryptoutility.hpp>

#include <publiq.pp/coin.hpp>

#include <unordered_set>
#include <unordered_map>
#include <exception>
#include <chrono>

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

    auto ptr_utl =
        beltpp::new_void_unique_ptr<beltpp::message_loader_utility>(std::move(utl));

    return ptr_utl;
}
static inline
beltpp::void_unique_ptr cm_get_putl()
{
    beltpp::message_loader_utility utl;
    CommanderMessage::detail::extension_helper(utl);

    auto ptr_utl =
        beltpp::new_void_unique_ptr<beltpp::message_loader_utility>(std::move(utl));

    return ptr_utl;
}

using TransactionLogLoader = daemon_rpc::TransactionLogLoader;
using RewardLogLoader = daemon_rpc::RewardLogLoader;
using LogIndexLoader = daemon_rpc::LogIndexLoader;

TransactionLogLoader daemon_rpc::get_transaction_log(std::string const& address)
{
    return
    TransactionLogLoader("tx",
                         meshpp::data_directory_path("accounts_log", address),
                         1000,
                         10,
                         bm_get_putl());
}
RewardLogLoader daemon_rpc::get_reward_log(std::string const& address)
{
    return
    RewardLogLoader("rw",
                    meshpp::data_directory_path("accounts_log", address),
                    1000,
                    10,
                    bm_get_putl());
}
LogIndexLoader daemon_rpc::get_transaction_log_index(std::string const& address)
{
    return
    LogIndexLoader("index_tx",
                   meshpp::data_directory_path("accounts_log", address),
                   1000,
                   cm_get_putl());
}
LogIndexLoader daemon_rpc::get_reward_log_index(std::string const& address)
{
    return
    LogIndexLoader("index_rw",
                   meshpp::data_directory_path("accounts_log", address),
                   1000,
                   cm_get_putl());
}

daemon_rpc::daemon_rpc()
    : eh()
    , socket(beltpp::getsocket<sf>(eh))
    , peerid()
    , log_index(meshpp::data_file_path("log_index.txt"))
{
    eh.add(socket);
}

void daemon_rpc::open(beltpp::ip_address const& connect_to_address)
{
    auto peerids = socket.open(connect_to_address);

    if (peerids.size() != 1)
        throw std::runtime_error(connect_to_address.to_string() + " is ambigous or unknown");

    bool keep_trying = true;
    while (keep_trying)
    {
        unordered_set<beltpp::ievent_item const*> wait_sockets;
        auto wait_result = eh.wait(wait_sockets);
        B_UNUSED(wait_sockets);

        if (wait_result == beltpp::event_handler::event)
        {
            peer_id _peerid;

            auto received_packets = socket.receive(_peerid);

            if (peerids.front() != _peerid)
                throw std::logic_error("logic error in open() - peerids.front() != peerid");

            for (auto& received_packet : received_packets)
            {
                packet& ref_packet = received_packet;

                switch (ref_packet.type())
                {
                case beltpp::isocket_join::rtt:
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

void daemon_rpc::close()
{
    if (peerid.empty())
        throw std::runtime_error("no daemon_rpc connection to close");

    socket.send(peerid, beltpp::isocket_drop());
}

enum class update_balance_type {increase, decrease};
void update_balance(string const& str_account,
                    BlockchainMessage::Coin const& update_by,
                    unordered_set<string> const& set_accounts,
                    meshpp::map_loader<CommanderMessage::Account>& accounts,
                    update_balance_type type)
{
    auto it = set_accounts.find(str_account);
    if (it != set_accounts.end())
    {
        {
            CommanderMessage::Account account;
            account.address = str_account;

            meshpp::public_key pb(account.address);

            accounts.insert(account.address, account);
        }

        auto& account = accounts.at(str_account);

        publiqpp::coin balance(account.balance.whole, account.balance.fraction);
        publiqpp::coin change(update_by);

        if (type == update_balance_type::decrease)
            balance -= change;
        else
            balance += change;

        balance.to_Coin(account.balance);
    }
}

void process_transaction(uint64_t block_index,
                         string const& str_account,
                         BlockchainMessage::TransactionLog const& transaction_log,
                         unordered_set<string> const& set_accounts,
                         unordered_map<string, TransactionLogLoader>& transactions,
                         unordered_map<string, LogIndexLoader>& index_transactions,
                         LoggingType type)
{
    auto it = set_accounts.find(str_account);
    if (it != set_accounts.end())
    {
        auto tl_insert_res = transactions.emplace(std::make_pair(str_account,
                                                                 daemon_rpc::get_transaction_log(str_account)));

        TransactionLogLoader& tlogloader = tl_insert_res.first->second;

        if (LoggingType::apply == type)
            tlogloader.push_back(transaction_log);
        else
            tlogloader.pop_back();

        auto idx_insert_res = index_transactions.emplace(std::make_pair(str_account,
                                                                        daemon_rpc::get_transaction_log_index(str_account)));

        string str_block_index = std::to_string(block_index);
        LogIndexLoader& idxlogloader = idx_insert_res.first->second;
        if (LoggingType::apply == type)
        {
            size_t current_tx_index = tlogloader.size() - 1;
            CommanderMessage::NumberPair value;
            value.first = current_tx_index;
            value.second = 1;

            if (false == idxlogloader.insert(str_block_index, value))
            {
                auto& stored_value = idxlogloader.at(str_block_index);
                bool verify = stored_value.first + stored_value.second == current_tx_index;
                assert(verify);
                ++stored_value.second;

                if (false == verify)
                    throw std::logic_error("cannot add to transaction index");
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
void process_reward(uint64_t block_index,
                    string const& str_account,
                    BlockchainMessage::RewardLog const& reward_log,
                    unordered_set<string> const& set_accounts,
                    unordered_map<string, RewardLogLoader>& rewards,
                    unordered_map<string, LogIndexLoader>& index_rewards,
                    LoggingType type)
{
    auto it = set_accounts.find(str_account);
    if (it != set_accounts.end())
    {
        auto rw_insert_res = rewards.emplace(std::make_pair(str_account,
                                                            daemon_rpc::get_reward_log(str_account)));

        RewardLogLoader& rlogloader = rw_insert_res.first->second;

        if (LoggingType::apply == type)
            rlogloader.push_back(reward_log);
        else
            rlogloader.pop_back();

        auto idx_insert_res = index_rewards.emplace(std::make_pair(str_account,
                                                                   daemon_rpc::get_reward_log_index(str_account)));

        string str_block_index = std::to_string(block_index);
        LogIndexLoader& idxlogloader = idx_insert_res.first->second;
        if (LoggingType::apply == type)
        {
            size_t current_rw_index = rlogloader.size() - 1;
            CommanderMessage::NumberPair value;
            value.first = current_rw_index;
            value.second = 1;

            if (false == idxlogloader.insert(str_block_index, value))
            {
                auto& stored_value = idxlogloader.at(str_block_index);
                bool verify = stored_value.first + stored_value.second == current_rw_index;
                assert(verify);
                ++stored_value.second;

                if (false == verify)
                    throw std::logic_error("cannot add to reward index");
            }
        }
        else
        {
            size_t current_rw_index = rlogloader.size();

            bool verify = idxlogloader.contains(str_block_index);
            assert(verify);

            if (false == verify)
                throw std::logic_error("cannot remove from reward index");

            auto& stored_value = idxlogloader.at(str_block_index);

            verify = stored_value.first + stored_value.second == current_rw_index + 1;
            assert(verify);
            if (false == verify)
                throw std::logic_error("cannot remove from reward index - check error");

            if (stored_value.second == 1)
                idxlogloader.erase(str_block_index);
            else
                --stored_value.second;
        }
    }
}

string daemon_rpc::send(CommanderMessage::Send const& send,
                        rpc& rpc_server)
{
    B_UNUSED(rpc_server);

    if (peerid.empty())
        throw std::runtime_error("no daemon_rpc connection to work");

    string transaction_hash;

    meshpp::private_key pv(send.private_key);
    meshpp::public_key pb(send.to);

    BlockchainMessage::Transfer tf;
    publiqpp::coin(send.amount).to_Coin(tf.amount);
    tf.from = pv.get_public_key().to_string();
    tf.message = send.message;
    tf.to = pb.to_string();

    BlockchainMessage::Transaction tx;
    tx.action = std::move(tf);
    publiqpp::coin(send.fee).to_Coin(tx.fee);
    tx.creation.tm = chrono::system_clock::to_time_t(chrono::system_clock::now());
    tx.expiry.tm =  chrono::system_clock::to_time_t(chrono::system_clock::now() + chrono::seconds(send.seconds_to_expire));

    BlockchainMessage::SignedTransaction stx;
    stx.authority = pv.get_public_key().to_string();
    stx.signature = pv.sign(tx.to_string()).base58;
    stx.transaction_details = std::move(tx);

    transaction_hash = meshpp::hash(stx.to_string());

    BlockchainMessage::Broadcast bc;
    bc.echoes = 2;
    bc.package = std::move(stx);

    socket.send(peerid, std::move(bc));

    bool keep_trying = true;
    while (keep_trying)
    {
        unordered_set<beltpp::ievent_item const*> wait_sockets;
        auto wait_result = eh.wait(wait_sockets);
        B_UNUSED(wait_sockets);

        if (wait_result == beltpp::event_handler::event)
        {
            peer_id _peerid;

            auto received_packets = socket.receive(_peerid);

            for (auto& received_packet : received_packets)
            {
                packet& ref_packet = received_packet;

                switch (ref_packet.type())
                {
                case BlockchainMessage::Done::rtt:
                {
                    peerid = _peerid;
                    keep_trying = false;
                    break;
                }
                default:
                    throw std::runtime_error("broadcast error");
                }

                if (false == keep_trying)
                    break;
            }
        }
    }

    return transaction_hash;
}

void daemon_rpc::sync(rpc& rpc_server,
                      unordered_set<string> const& set_accounts,
                      bool const new_import)
{
    if (peerid.empty())
        throw std::runtime_error("no daemon_rpc connection to work");

    unordered_map<string, TransactionLogLoader> transactions;
    unordered_map<string, RewardLogLoader> rewards;
    unordered_map<string, LogIndexLoader> index_transactions;
    unordered_map<string, LogIndexLoader> index_rewards;


    beltpp::on_failure discard([this,
                               &rpc_server,
                               &transactions,
                               &rewards,
                               &index_transactions,
                               &index_rewards]()
    {
        log_index.discard();
        rpc_server.accounts.discard();
        rpc_server.blocks.discard();
        rpc_server.head_block_index.discard();

        for (auto& tr : transactions)
            tr.second.discard();
        for (auto& rw : rewards)
            rw.second.discard();
        for (auto& tr : index_transactions)
            tr.second.discard();
        for (auto& rw : index_rewards)
            rw.second.discard();
    });

    uint64_t local_start_index = 0;
    uint64_t local_head_block_index = 0;

    auto start_index = [new_import, this, &local_start_index]()
    {
        if (new_import)
            return local_start_index;
        return log_index.as_const()->value;
    };

    auto set_start_index = [new_import, this, &local_start_index](uint64_t index)
    {
        if (new_import)
            local_start_index = index;
        else
            log_index->value = index;
    };

    auto head_block_index = [new_import, &rpc_server, &local_head_block_index]()
    {
        if (new_import)
            return local_head_block_index;
        else
            return rpc_server.head_block_index->value;
    };
    auto increment_head_block_index = [new_import, &rpc_server, &local_head_block_index]()
    {
        if (new_import)
            ++local_head_block_index;
        else
            ++rpc_server.head_block_index->value;
    };
    auto decrement_head_block_index = [new_import, &rpc_server, &local_head_block_index]()
    {
        if (new_import)
            --local_head_block_index;
        else
            --rpc_server.head_block_index->value;
    };

    while (true)
    {
        size_t const max_count = 3000;
        LoggedTransactionsRequest req;
        req.max_count = max_count;
        req.start_index = start_index();
        socket.send(peerid, req);

        size_t count = 0;
        bool new_import_done = false;

        while (true)
        {
            unordered_set<beltpp::ievent_item const*> wait_sockets;
            auto wait_result = eh.wait(wait_sockets);
            B_UNUSED(wait_sockets);

            if (wait_result == beltpp::event_handler::event)
            {
                peer_id _peerid;

                auto received_packets = socket.receive(_peerid);

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
                            if (start_index() == 0)
                                dont_increment_head_block_index = true;

                            set_start_index(action_info.index + 1);

                            auto action_type = action_info.action.type();

                            if (action_info.logging_type == LoggingType::apply)
                            {
                                if (action_type == BlockLog::rtt)
                                {
                                    if (false == dont_increment_head_block_index)
                                        increment_head_block_index();

                                    BlockLog block_log;
                                    std::move(action_info.action).get(block_log);

                                    if (!new_import)
                                    {
                                        CommanderMessage::BlockInfo block_info;
                                        block_info.authority = block_log.authority;
                                        block_info.block_hash = block_log.block_hash;
                                        block_info.block_number = block_log.block_number;
                                        block_info.block_size = block_log.block_size;
                                        block_info.time_signed.tm = block_log.time_signed.tm;
                                        rpc_server.blocks.push_back(block_info);
                                    }

                                    uint64_t block_index = head_block_index();

                                    for (auto& transaction_log: block_log.transactions)
                                    {
                                        ++count;

                                        if (transaction_log.action.type() == Transfer::rtt)
                                        {
                                            Transfer tf;
                                            transaction_log.action.get(tf);

                                            update_balance(tf.from,
                                                           tf.amount,
                                                           set_accounts,
                                                           rpc_server.accounts,
                                                           update_balance_type::decrease);

                                            update_balance(tf.to,
                                                           tf.amount,
                                                           set_accounts,
                                                           rpc_server.accounts,
                                                           update_balance_type::increase);
                                            update_balance(tf.from,
                                                           transaction_log.fee,
                                                           set_accounts,
                                                           rpc_server.accounts,
                                                           update_balance_type::decrease);
                                            update_balance(block_log.authority,
                                                           transaction_log.fee,
                                                           set_accounts,
                                                           rpc_server.accounts,
                                                           update_balance_type::increase);

                                            process_transaction(block_index,
                                                                tf.from,
                                                                transaction_log,
                                                                set_accounts,
                                                                transactions,
                                                                index_transactions,
                                                                LoggingType::apply);
                                            if (tf.to != tf.from)
                                            process_transaction(block_index,
                                                                tf.to,
                                                                transaction_log,
                                                                set_accounts,
                                                                transactions,
                                                                index_transactions,
                                                                LoggingType::apply);
                                            if (block_log.authority != tf.to &&
                                                block_log.authority != tf.from)
                                            process_transaction(block_index,
                                                                block_log.authority,
                                                                transaction_log,
                                                                set_accounts,
                                                                transactions,
                                                                index_transactions,
                                                                LoggingType::apply);
                                        }
                                        else
                                        {
//                                            assert(false);
//                                            throw std::logic_error("unknown transaction log item - " +
//                                                                   std::to_string(action_type));
                                        }
                                    }

                                    for (auto& reward_info : block_log.rewards)
                                    {
                                        B_UNUSED(reward_info);
                                        ++count;

                                        update_balance(reward_info.to,
                                                       reward_info.amount,
                                                       set_accounts,
                                                       rpc_server.accounts,
                                                       update_balance_type::increase);

                                        process_reward(block_index,
                                                       reward_info.to,
                                                       reward_info,
                                                       set_accounts,
                                                       rewards,
                                                       index_rewards,
                                                       LoggingType::apply);
                                    }
                                }
                                else if (action_type == TransactionLog::rtt)
                                {
                                    uint64_t block_index = head_block_index() + 1;

                                    TransactionLog transaction_log;
                                    std::move(action_info.action).get(transaction_log);

                                    if (transaction_log.action.type() == Transfer::rtt)
                                    {
                                        Transfer tf;
                                        transaction_log.action.get(tf);

                                        update_balance(tf.from,
                                                       tf.amount,
                                                       set_accounts,
                                                       rpc_server.accounts,
                                                       update_balance_type::decrease);
                                        update_balance(tf.to,
                                                       tf.amount,
                                                       set_accounts,
                                                       rpc_server.accounts,
                                                       update_balance_type::increase);
                                        update_balance(tf.from,
                                                       transaction_log.fee,
                                                       set_accounts,
                                                       rpc_server.accounts,
                                                       update_balance_type::decrease);


                                        process_transaction(block_index,
                                                            tf.from,
                                                            transaction_log,
                                                            set_accounts,
                                                            transactions,
                                                            index_transactions,
                                                            LoggingType::apply);
                                        if (tf.to != tf.from)
                                        process_transaction(block_index,
                                                            tf.to,
                                                            transaction_log,
                                                            set_accounts,
                                                            transactions,
                                                            index_transactions,
                                                            LoggingType::apply);
                                    }
                                    else
                                    {
//                                        assert(false);
//                                        throw std::logic_error("unknown transaction log item - " +
//                                                               std::to_string(action_type));
                                    }
                                }
                                else
                                {
                                    assert(false);
                                    throw std::logic_error("unknown log item - " +
                                                           std::to_string(action_type));
                                }
                            }
                            else// if (action_info.logging_type == LoggingType::revert)
                            {
                                if (action_type == BlockLog::rtt)
                                {
                                    uint64_t block_index = head_block_index();

                                    decrement_head_block_index();

                                    BlockLog block_log;
                                    std::move(action_info.action).get(block_log);



                                    for (auto& transaction_log : block_log.transactions)
                                    {
                                        ++count;

                                        if (transaction_log.action.type() == Transfer::rtt)
                                        {
                                            Transfer tf;
                                            transaction_log.action.get(tf);

                                            update_balance(tf.from,
                                                           tf.amount,
                                                           set_accounts,
                                                           rpc_server.accounts,
                                                           update_balance_type::increase);
                                            update_balance(tf.to,
                                                           tf.amount,
                                                           set_accounts,
                                                           rpc_server.accounts,
                                                           update_balance_type::decrease);
                                            update_balance(tf.from,
                                                           transaction_log.fee,
                                                           set_accounts,
                                                           rpc_server.accounts,
                                                           update_balance_type::increase);
                                            update_balance(block_log.authority,
                                                           transaction_log.fee,
                                                           set_accounts,
                                                           rpc_server.accounts,
                                                           update_balance_type::decrease);

                                            process_transaction(block_index,
                                                                tf.from,
                                                                transaction_log,
                                                                set_accounts,
                                                                transactions,
                                                                index_transactions,
                                                                LoggingType::revert);
                                            if (tf.to != tf.from)
                                            process_transaction(block_index,
                                                                tf.to,
                                                                transaction_log,
                                                                set_accounts,
                                                                transactions,
                                                                index_transactions,
                                                                LoggingType::revert);
                                            if (block_log.authority != tf.to &&
                                                block_log.authority != tf.from)
                                            process_transaction(block_index,
                                                                block_log.authority,
                                                                transaction_log,
                                                                set_accounts,
                                                                transactions,
                                                                index_transactions,
                                                                LoggingType::revert);
                                        }
                                        else
                                        {
//                                            assert(false);
//                                            throw std::logic_error("unknown transaction log item - " +
//                                                                   std::to_string(action_type));
                                        }
                                    }

                                    for (auto& reward_info : block_log.rewards)
                                    {
                                        B_UNUSED(reward_info);
                                        ++count;

                                        update_balance(reward_info.to,
                                                       reward_info.amount,
                                                       set_accounts,
                                                       rpc_server.accounts,
                                                       update_balance_type::decrease);

                                        process_reward(block_index,
                                                       reward_info.to,
                                                       reward_info,
                                                       set_accounts,
                                                       rewards,
                                                       index_rewards,
                                                       LoggingType::revert);
                                    }

                                }
                                else if (action_type == TransactionLog::rtt)
                                {
                                    TransactionLog transaction_log;
                                    std::move(action_info.action).get(transaction_log);

                                    uint64_t block_index = head_block_index() + 1;

                                    if (transaction_log.action.type() == Transfer::rtt)
                                    {
                                        Transfer tf;
                                        transaction_log.action.get(tf);

                                        update_balance(tf.from,
                                                       tf.amount,
                                                       set_accounts,
                                                       rpc_server.accounts,
                                                       update_balance_type::increase);
                                        update_balance(tf.to,
                                                       tf.amount,
                                                       set_accounts,
                                                       rpc_server.accounts,
                                                       update_balance_type::decrease);
                                        update_balance(tf.from,
                                                       transaction_log.fee,
                                                       set_accounts,
                                                       rpc_server.accounts,
                                                       update_balance_type::increase);

                                        process_transaction(block_index,
                                                            tf.from,
                                                            transaction_log,
                                                            set_accounts,
                                                            transactions,
                                                            index_transactions,
                                                            LoggingType::revert);
                                        if (tf.to != tf.from)
                                        process_transaction(block_index,
                                                            tf.to,
                                                            transaction_log,
                                                            set_accounts,
                                                            transactions,
                                                            index_transactions,
                                                            LoggingType::revert);
                                    }
                                    else
                                    {
//                                        assert(false);
//                                        throw std::logic_error("unknown transaction log item - " +
//                                                               std::to_string(action_type));
                                    }
                                }
                                else
                                {
                                    assert(false);
                                    throw std::logic_error("unknown log item - " +
                                                           std::to_string(action_type));
                                }
                                if (!new_import)
                                {
                                    rpc_server.blocks.pop_back();
                                }
                            }
                            if (new_import && local_start_index == log_index.as_const()->value)
                            {
                                new_import_done = true;
                                break;  //  breaks for()
                            }
                        }// for (auto& action_info : msg.actions)
                        break;  //  breaks switch case
                    }
                    default:
                        throw std::runtime_error(std::to_string(ref_packet.type()) + " - sync cannot handle");
                    }
                }

                if (false == received_packets.empty())
                    break;  //  breaks while() that calls receive()
            }
        }

        if ((false == new_import && count < max_count) ||
            new_import_done)
            break;
    }

    rpc_server.head_block_index.save();
    rpc_server.accounts.save();
    rpc_server.blocks.save();
    log_index.save();

    for (auto& tr : transactions)
        tr.second.save();
    for (auto& rw : rewards)
        rw.second.save();
    for (auto& tr : index_transactions)
        tr.second.save();
    for (auto& rw : index_rewards)
        rw.second.save();

    discard.dismiss();

    rpc_server.head_block_index.commit();
    rpc_server.accounts.commit();
    rpc_server.blocks.commit();
    log_index.commit();

    for (auto& tr : transactions)
        tr.second.commit();
    for (auto& rw : rewards)
        rw.second.commit();
    for (auto& tr : index_transactions)
        tr.second.commit();
    for (auto& rw : index_rewards)
        rw.second.commit();
}
