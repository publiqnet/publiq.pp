#include "daemon_rpc.hpp"
#include "rpc.hpp"

#include <belt.pp/socket.hpp>
#include <belt.pp/scope_helper.hpp>

#include <mesh.pp/settings.hpp>
#include <mesh.pp/cryptoutility.hpp>

#include <publiq.pp/coin.hpp>
#include <publiq.pp/message.hpp>

#include <unordered_set>
#include <unordered_map>
#include <exception>

using beltpp::packet;
using peer_id = beltpp::socket::peer_id;
using std::unordered_set;
using std::unordered_map;
using namespace BlockchainMessage;
using std::string;

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

    while (true)
    {
        unordered_set<beltpp::ievent_item const*> wait_sockets;
        auto wait_result = eh.wait(wait_sockets);
        B_UNUSED(wait_sockets);

        if (wait_result == beltpp::event_handler::event)
        {
            peer_id peerid;

            auto received_packets = socket.receive(peerid);

            if (peerids.front() != peerid)
                std::logic_error("logic error in open() - peerids.front() != peerid");

            for (auto& received_packet : received_packets)
            {
                packet& ref_packet = received_packet;

                switch (ref_packet.type())
                {
                case beltpp::isocket_join::rtt:
                {
                    this->peerid = peerid;
                    return;
                }
                default:
                    throw std::runtime_error(connect_to_address.to_string() + " cannot open");
                }
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

        account.balance = balance.to_Coin();
    }
}

using TransactionLogLoader = meshpp::vector_loader<BlockchainMessage::TransactionLog>;
using RewardLogLoader = meshpp::vector_loader<BlockchainMessage::RewardLog>;

void process_transaction(string const& str_account,
                         BlockchainMessage::TransactionLog const& transaction_log,
                         unordered_set<string> const& set_accounts,
                         unordered_map<string, TransactionLogLoader>& transactions,
                         LoggingType type)
{
    auto it = set_accounts.find(str_account);
    if (it != set_accounts.end())
    {
        auto insert_res = transactions.emplace(std::make_pair(str_account,
                                                              TransactionLogLoader("tx",
                                                                                   meshpp::data_directory_path("accounts_log", str_account),
                                                                                   100,
                                                                                   10,
                                                                                   bm_get_putl())));

        TransactionLogLoader& tlogloader = insert_res.first->second;

        if (LoggingType::apply == type)
            tlogloader.push_back(transaction_log);
        else
            tlogloader.pop_back();
    }
}
void process_reward(string const& str_account,
                    BlockchainMessage::RewardLog const& reward_log,
                    unordered_set<string> const& set_accounts,
                    unordered_map<string, RewardLogLoader>& rewards,
                    LoggingType type)
{
    auto it = set_accounts.find(str_account);
    if (it != set_accounts.end())
    {
        auto insert_res = rewards.emplace(std::make_pair(str_account,
                                                         RewardLogLoader("rw",
                                                                         meshpp::data_directory_path("accounts_log", str_account),
                                                                         100,
                                                                         10,
                                                                         bm_get_putl())));

        RewardLogLoader& rlogloader = insert_res.first->second;

        if (LoggingType::apply == type)
            rlogloader.push_back(reward_log);
        else
            rlogloader.pop_back();
    }
}

void daemon_rpc::sync(rpc& rpc_server,
                      unordered_set<string> const& set_accounts,
                      bool const new_import)
{
    if (peerid.empty())
        throw std::runtime_error("no daemon_rpc connection to close");

    unordered_map<string, TransactionLogLoader> transactions;
    unordered_map<string, RewardLogLoader> rewards;

    beltpp::on_failure discard([this, &rpc_server, &transactions, &rewards]()
    {
        log_index.discard();
        rpc_server.accounts.discard();
        rpc_server.head_block_index.discard();

        for (auto& tr : transactions)
            tr.second.discard();
        for (auto& rw : rewards)
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
                peer_id peerid;

                auto received_packets = socket.receive(peerid);

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

                            set_start_index(action_info.index + 1);

                            auto action_type = action_info.action.type();

                            if (action_info.logging_type == LoggingType::apply)
                            {
                                if (action_type == BlockLog::rtt)
                                {
                                    increment_head_block_index();

                                    BlockLog block_log;
                                    std::move(action_info.action).get(block_log);

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

                                            process_transaction(tf.from,
                                                                transaction_log,
                                                                set_accounts,
                                                                transactions,
                                                                LoggingType::apply);
                                            process_transaction(tf.to,
                                                                transaction_log,
                                                                set_accounts,
                                                                transactions,
                                                                LoggingType::apply);
                                            process_transaction(block_log.authority,
                                                                transaction_log,
                                                                set_accounts,
                                                                transactions,
                                                                LoggingType::apply);
                                        }
                                        else
                                        {
                                            assert(false);
                                            throw std::logic_error("unknown transaction log item - " +
                                                                   std::to_string(action_type));
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

                                        process_reward(reward_info.to,
                                                       reward_info,
                                                       set_accounts,
                                                       rewards,
                                                       LoggingType::apply);
                                    }
                                }
                                else if (action_type == TransactionLog::rtt)
                                {
                                    TransactionLog transaction_log;
                                    std::move(action_info.action).get(transaction_log);

                                    if (transaction_log.action.type() == Transfer::rtt)
                                    {
                                        Transfer tf;
                                        std::move(transaction_log.action).get(tf);

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


                                        process_transaction(tf.from,
                                                            transaction_log,
                                                            set_accounts,
                                                            transactions,
                                                            LoggingType::apply);
                                        process_transaction(tf.to,
                                                            transaction_log,
                                                            set_accounts,
                                                            transactions,
                                                            LoggingType::apply);
                                    }
                                    else
                                    {
                                        assert(false);
                                        throw std::logic_error("unknown transaction log item - " +
                                                               std::to_string(action_type));
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
                                    decrement_head_block_index();

                                    BlockLog block_log;
                                    std::move(action_info.action).get(block_log);

                                    for (auto& transaction_log : block_log.transactions)
                                    {
                                        ++count;

                                        if (transaction_log.action.type() == Transfer::rtt)
                                        {
                                            Transfer tf;
                                            std::move(transaction_log.action).get(tf);

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

                                            process_transaction(tf.from,
                                                                transaction_log,
                                                                set_accounts,
                                                                transactions,
                                                                LoggingType::revert);
                                            process_transaction(tf.to,
                                                                transaction_log,
                                                                set_accounts,
                                                                transactions,
                                                                LoggingType::revert);
                                            process_transaction(block_log.authority,
                                                                transaction_log,
                                                                set_accounts,
                                                                transactions,
                                                                LoggingType::revert);
                                        }
                                        else
                                        {
                                            assert(false);
                                            throw std::logic_error("unknown transaction log item - " +
                                                                   std::to_string(action_type));
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

                                        process_reward(reward_info.to,
                                                       reward_info,
                                                       set_accounts,
                                                       rewards,
                                                       LoggingType::revert);
                                    }
                                }
                                else if (action_type == TransactionLog::rtt)
                                {
                                    TransactionLog transaction_log;
                                    std::move(action_info.action).get(transaction_log);

                                    if (transaction_log.action.type() == Transfer::rtt)
                                    {
                                        Transfer tf;
                                        std::move(transaction_log.action).get(tf);

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

                                        process_transaction(tf.from,
                                                            transaction_log,
                                                            set_accounts,
                                                            transactions,
                                                            LoggingType::revert);
                                        process_transaction(tf.to,
                                                            transaction_log,
                                                            set_accounts,
                                                            transactions,
                                                            LoggingType::revert);
                                    }
                                    else
                                    {
                                        assert(false);
                                        throw std::logic_error("unknown transaction log item - " +
                                                               std::to_string(action_type));
                                    }
                                }
                                else
                                {
                                    assert(false);
                                    throw std::logic_error("unknown log item - " +
                                                           std::to_string(action_type));
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
    log_index.save();

    for (auto& tr : transactions)
        tr.second.save();
    for (auto& rw : rewards)
        rw.second.save();

    discard.dismiss();

    rpc_server.head_block_index.commit();
    rpc_server.accounts.commit();
    log_index.commit();


    for (auto& tr : transactions)
        tr.second.commit();
    for (auto& rw : rewards)
        rw.second.commit();
}
