#include "daemon_rpc.hpp"
#include "rpc.hpp"
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
#include <iostream>

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

        if (wait_result & beltpp::event_handler::event)
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

    socket.send(peerid, beltpp::packet(beltpp::isocket_drop()));
}

enum class update_balance_type {increase, decrease};
void update_balance(string const& str_account,
                    BlockchainMessage::Coin const& update_by,
                    unordered_set<string> const& set_accounts,
                    meshpp::map_loader<CommanderMessage::Account>& accounts,
                    update_balance_type type)
{
    if (set_accounts.count(str_account))
    {
        {
            CommanderMessage::Account account;
            account.address = str_account;

            meshpp::public_key pb(account.address);

            accounts.insert(account.address, account);
        }

        auto& account = accounts.at(str_account);

        publiqpp::coin balance(account.balance);
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
    if (set_accounts.count(str_account))
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

void process_reward(uint64_t block_index,
                    string const& str_account,
                    BlockchainMessage::RewardLog const& reward_log,
                    unordered_set<string> const& set_accounts,
                    unordered_map<string, RewardLogLoader>& rewards,
                    unordered_map<string, LogIndexLoader>& index_rewards,
                    LoggingType type)
{
    if (set_accounts.count(str_account))
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
                if (false == verify)
                    throw std::logic_error("cannot add to reward index");

                ++stored_value.second;
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

void update_balances(unordered_set<string> const& set_accounts,
                    rpc& rpc_server,
                    TransactionLog const& transaction_log,
                    string  const& authority,
                    LoggingType type)
{
    const TransactionInfo transaction_info = TransactionInfo(transaction_log);

    if (LoggingType::apply == type)
    {
        if (Coin() != transaction_info.amount)
        {
            if (!transaction_info.from.empty())
                update_balance(transaction_info.from,
                               transaction_info.amount,
                               set_accounts,
                               rpc_server.accounts,
                               update_balance_type::decrease);

            if (!transaction_info.to.empty())
                update_balance(transaction_info.to,
                               transaction_info.amount,
                               set_accounts,
                               rpc_server.accounts,
                               update_balance_type::increase);
        }

        if (Coin() != transaction_info.fee)
        {
            if (!transaction_info.from.empty())
                update_balance(transaction_info.from,
                               transaction_info.fee,
                               set_accounts,
                               rpc_server.accounts,
                               update_balance_type::decrease);

            if (!authority.empty())
                update_balance(authority,
                               transaction_info.fee,
                               set_accounts,
                               rpc_server.accounts,
                               update_balance_type::increase);
        }
    }
    else //if (LoggingType::revert == type)
    {
        if (Coin() != transaction_info.amount)
        {
            if (!transaction_info.from.empty())
                update_balance(transaction_info.from,
                               transaction_info.amount,
                               set_accounts,
                               rpc_server.accounts,
                               update_balance_type::increase);

            if (!transaction_info.to.empty())
                update_balance(transaction_info.to,
                               transaction_info.amount,
                               set_accounts,
                               rpc_server.accounts,
                               update_balance_type::decrease);
        }

        if (Coin() != transaction_info.fee)
        {
            if (!transaction_info.from.empty())
                update_balance(transaction_info.from,
                               transaction_info.fee,
                               set_accounts,
                               rpc_server.accounts,
                               update_balance_type::increase);

            if (!authority.empty())
                update_balance(authority,
                               transaction_info.fee,
                               set_accounts,
                               rpc_server.accounts,
                               update_balance_type::decrease);
        }
    }

}

void process_transactions(uint64_t block_index,
                          BlockchainMessage::TransactionLog const& transaction_log,
                          unordered_set<string> const& set_accounts,
                          unordered_map<string, TransactionLogLoader>& transactions,
                          unordered_map<string, LogIndexLoader>& index_transactions,
                          string  const& authority,
                          LoggingType type)
{

    const TransactionInfo transaction_info = TransactionInfo(transaction_log);

    if (!transaction_info.from.empty())
        process_transaction(block_index,
                            transaction_info.from,
                            transaction_log,
                            set_accounts,
                            transactions,
                            index_transactions,
                            type);

    if (!authority.empty() &&
         authority != transaction_info.from)
        process_transaction(block_index,
                            authority,
                            transaction_log,
                            set_accounts,
                            transactions,
                            index_transactions,
                            type);

    if (!transaction_info.to.empty() &&
         transaction_info.to != transaction_info.from &&
         transaction_info.to != authority)
        process_transaction(block_index,
                            transaction_info.to,
                            transaction_log,
                            set_accounts,
                            transactions,
                            index_transactions,
                            type);
}

void process_storage_tansactions(unordered_set<string> const& set_accounts,
                                 BlockchainMessage::TransactionLog const& transaction_log,
                                 rpc& rpc_server,
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

            if (!rpc_server.storages.contains(storage_update.storage_address))
            {
                CommanderMessage::StoragesResponseItem storage;
                storage.storage_address = storage_update.storage_address;
                storage.file_uris[storage_update.file_uri] = is_stored;

                rpc_server.storages.insert(storage_update.storage_address, storage);
            }
            else
            {
                auto& storage = rpc_server.storages.at(storage_update.storage_address);
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

void process_channel_tansactions(unordered_set<string> const& set_accounts,
                                 BlockchainMessage::TransactionLog const& transaction_log,
                                 rpc& rpc_server,
                                 LoggingType type)
{
    if (ContentUnit::rtt == transaction_log.action.type())
    {
        ContentUnit content_unit;
        transaction_log.action.get(content_unit);

        if (set_accounts.count(content_unit.channel_address))
        {
            if (LoggingType::apply == type)
            {
                if (!rpc_server.channels.contains(content_unit.channel_address))
                {
                    CommanderMessage::ContentUnit cont_unit;
                    cont_unit.author_addresses = content_unit.author_addresses;
                    cont_unit.file_uris = content_unit.file_uris;

                    CommanderMessage::Content content;
                    content.approved = false;
                    content.content_units[content_unit.uri] = cont_unit;

                    CommanderMessage::Contents contents;
                    contents.content_histories.push_back(content);

                    CommanderMessage::ChannelsResponseItem channel_response_item;
                    channel_response_item.channel_address = content_unit.channel_address;
                    channel_response_item.contents[content_unit.content_id] = contents;

                    rpc_server.channels.insert(content_unit.channel_address, channel_response_item);
                }
                else
                {
                    auto& channel = rpc_server.channels.at(content_unit.channel_address);
                    auto contents_item = channel.contents.find(content_unit.content_id);
                    if (contents_item == channel.contents.end())
                    {
                        CommanderMessage::ContentUnit cont_unit;
                        cont_unit.author_addresses = content_unit.author_addresses;
                        cont_unit.file_uris = content_unit.file_uris;

                        CommanderMessage::Content content;
                        content.approved = false;
                        content.content_units[content_unit.uri] = cont_unit;

                        CommanderMessage::Contents contents;
                        contents.content_histories.push_back(content);

                        channel.contents[content_unit.content_id] = contents;
                    }
                    else
                    {
                        auto& content_histories = contents_item->second.content_histories;
                        assert (!content_histories.empty());
                        if (content_histories.empty())
                            throw std::logic_error("content_histories.empty()");

                        auto& content = content_histories.back();
                        if (!content.approved)
                        {
                            CommanderMessage::ContentUnit cont_unit;
                            cont_unit.author_addresses = content_unit.author_addresses;
                            cont_unit.file_uris = content_unit.file_uris;

                            content.content_units[content_unit.uri] = cont_unit;
                        }
                        else
                        {
                            CommanderMessage::ContentUnit cont_unit;
                            cont_unit.author_addresses = content_unit.author_addresses;
                            cont_unit.file_uris = content_unit.file_uris;

                            CommanderMessage::Content _content;
                            _content.approved = false;
                            _content.content_units[content_unit.uri] = cont_unit;

                            content_histories.push_back(_content);
                        }
                    }
                }
            }
            else //if (LoggingType::revert == type)
            {
                assert (rpc_server.channels.contains(content_unit.channel_address));
                auto& channel_response_item = rpc_server.channels.at(content_unit.channel_address);
                if (!rpc_server.channels.contains(content_unit.channel_address))
                    throw std::logic_error("rpc_server.channels.contains(content_unit.channel_address)");

                auto contents_item = channel_response_item.contents.find(content_unit.content_id);
                assert (contents_item != channel_response_item.contents.end());
                if (contents_item == channel_response_item.contents.end())
                    throw std::logic_error("contents_item == channel_response_item.contents.end()");

                auto& contents = contents_item->second.content_histories;
                assert (contents.size() != 0);
                if (contents.size() == 0)
                    throw std::logic_error("contents.size() == 0");

                size_t erased_count = contents.back().content_units.erase(content_unit.uri);

                assert(erased_count != 0);
                if (erased_count == 0)
                    throw std::logic_error("erased_count == 0");

                assert(contents.back().approved == false);
                if (contents.back().approved)
                    throw std::logic_error("contents.back().approved");

                if (contents.back().content_units.empty())
                    contents.pop_back();

                if (contents.empty())
                    erased_count = channel_response_item.contents.erase(content_unit.content_id);

                assert(erased_count != 0);
                if (erased_count == 0)
                    throw std::logic_error("erased_count == 0");

                if (channel_response_item.contents.empty())
                    erased_count = rpc_server.channels.erase(content_unit.channel_address);

                assert(erased_count != 0);
                if (erased_count == 0)
                    throw std::logic_error("erased_count == 0");
            }
        }
    }

    if (Content::rtt == transaction_log.action.type())
    {
        Content content;
        transaction_log.action.get(content);

        if (set_accounts.count(content.channel_address))
        {
            if (LoggingType::apply == type)
            {
                assert (rpc_server.channels.contains(content.channel_address));
                if (!rpc_server.channels.contains(content.channel_address))
                    throw std::logic_error("rpc_server.channels.contains(content.channel_address)");

                auto& channel = rpc_server.channels.at(content.channel_address);
                auto contents = channel.contents.find(content.content_id);
                assert (contents != channel.contents.end());
                if (contents == channel.contents.end())
                    throw std::logic_error("contents_item == channel_response_item.contents.end()");

                auto& content_histories = contents->second.content_histories;
                assert (content_histories.size() != 0);
                if (content_histories.size() == 0)
                    throw std::logic_error("content_histories.size() == 0");

                CommanderMessage::Content new_content;

                for (auto& content_unit_uri : content.content_unit_uris)
                {
                    bool found = false;
                    for (auto rit = content_histories.rbegin(); rit != content_histories.rend(); ++rit)
                    {
                        if (rit->content_units.count(content_unit_uri))
                        {
                            found = true;
                            new_content.content_units[content_unit_uri] = rit->content_units[content_unit_uri];

                            if (rit == content_histories.rbegin() &&
                                !rit->approved)
                                    rit->content_units.erase(content_unit_uri);
                            break;
                        }
                    }
                    assert(found);
                    if (!found)
                        throw std::logic_error ("!found");
                }
                new_content.approved = true;

                assert (new_content.content_units.size() != 0);
                if (new_content.content_units.size() == 0)
                    throw std::logic_error("new_content.content_units.size() == 0");

                if (content_histories.back().content_units.empty())
                    content_histories.pop_back();

                if (content_histories.empty())
                    content_histories.push_back(std::move(new_content));
                else if (content_histories.size() == 1 &&
                         !content_histories.back().approved)
                    content_histories.insert(content_histories.begin(), std::move(new_content));
                else
                {
                    for ( auto it = content_histories.begin(); it != content_histories.end(); ++it)
                    {
                        if (it->approved)
                        {
                            it->approved = false;
                            content_histories.insert(it + 1, std::move(new_content));
                            break;
                        }
                    }
                }
            }
            else //if (LoggingType::revert == type)
            {
                assert (rpc_server.channels.contains(content.channel_address));
                if (!rpc_server.channels.contains(content.channel_address))
                    throw std::logic_error("rpc_server.channels.contains(content.channel_address)");

                auto& channel = rpc_server.channels.at(content.channel_address);
                auto contents = channel.contents.find(content.content_id);
                assert (contents != channel.contents.end());
                if (contents == channel.contents.end())
                    throw std::logic_error("contents_item == channel_response_item.contents.end()");

                auto& content_histories = contents->second.content_histories;
                assert (content_histories.size() != 0);
                if (content_histories.size() == 0)
                    throw std::logic_error("content_histories.size() == 0");

                auto approved_iter = content_histories.begin();
                for ( ; approved_iter != content_histories.end(); ++approved_iter)
                {
                    if (approved_iter->approved)
                    {
                        size_t count = 0;
                        for (auto& content_unit_uri : content.content_unit_uris)
                        {
                            if (approved_iter->content_units.count(content_unit_uri))
                                ++count;
                        }
                        assert(count == content.content_unit_uris.size());
                        if (count != content.content_unit_uris.size())
                            throw std::logic_error ("count != content.content_unit_uris.size()");

                        break;
                    }
                }
                assert (approved_iter != content_histories.end());
                if (approved_iter == content_histories.end())
                    throw std::logic_error("approved_iter == content_histories.end()");

                assert (approved_iter->approved == true);
                if (approved_iter->approved != true)
                    throw std::logic_error("approved_iter->approved != true");

                for (auto it = content_histories.begin(); it != approved_iter; ++it)
                {
                    assert(!it->approved);
                    if (it->approved)
                        throw std::logic_error("it->approved");

                    for (auto& content_unit : it->content_units)
                        approved_iter->content_units.erase(content_unit.first);

                    if (it + 1 == approved_iter)
                        it->approved = true;
                }
                if (approved_iter == content_histories.end() - 1)
                    approved_iter->approved = false;
                else
                {
                    for (auto& content_unit : approved_iter->content_units)
                    {
                        auto emplace_result = content_histories.back().content_units.emplace(std::move(content_unit));

                        assert(emplace_result.second);
                        if (!emplace_result.second)
                            throw std::logic_error("!emplace_result.second");
                    }
                    content_histories.erase(approved_iter);
                }
            }
        }
    }
}

void process_statistics_tansactions(BlockchainMessage::TransactionLog const& transaction_log,
                                    rpc& rpc_server,
                                    LoggingType type)
{
    if (ServiceStatistics::rtt == transaction_log.action.type())
    {
        uint64_t block_number = rpc_server.head_block_index.as_const()->value;

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
                        rpc_server.m_file_usage_map[block_number][file_item.file_uri] += count_item.count;
                }
            }
        }
        else //if (LoggingType::revert == type)
        {
            if (rpc_server.m_file_usage_map.find(block_number) != rpc_server.m_file_usage_map.end())
                rpc_server.m_file_usage_map.erase(block_number);
        }
    }
}

beltpp::packet daemon_rpc::process_storage_update_request(CommanderMessage::StorageUpdateRequest const& update,
                                                          rpc& rpc_server)
{
    B_UNUSED(rpc_server);

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

    socket.send(peerid, beltpp::packet(std::move(bc)));

    bool keep_trying = true;
    while (keep_trying)
    {
        unordered_set<beltpp::ievent_item const*> wait_sockets;
        auto wait_result = eh.wait(wait_sockets);
        B_UNUSED(wait_sockets);

        if (wait_result & beltpp::event_handler::event)
        {
            peer_id _peerid;

            auto received_packets = socket.receive(_peerid);

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
                case beltpp::isocket_drop::rtt:
                {
                    CommanderMessage::Failed response;
                    response.message = "server disconnected";
                    response.reason = std::move(ref_packet);
                    result = std::move(response);
                    keep_trying = false;
                    break;
                }
                default:
                {
                    CommanderMessage::Failed response;
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

beltpp::packet daemon_rpc::send(CommanderMessage::Send const& send,
                                rpc& rpc_server)
{
    B_UNUSED(rpc_server);

    if (peerid.empty())
        throw std::runtime_error("no daemon_rpc connection to work");

    beltpp::packet result;
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

    socket.send(peerid, beltpp::packet(std::move(bc)));

    bool keep_trying = true;
    while (keep_trying)
    {
        unordered_set<beltpp::ievent_item const*> wait_sockets;
        auto wait_result = eh.wait(wait_sockets);
        B_UNUSED(wait_sockets);

        if (wait_result & beltpp::event_handler::event)
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
                    CommanderMessage::StringValue response;
                    response.value = transaction_hash;
                    result = std::move(response);
                    peerid = _peerid;
                    keep_trying = false;
                    break;
                }
                case beltpp::isocket_drop::rtt:
                {
                    CommanderMessage::Failed response;
                    response.message = "server disconnected";
                    response.reason = std::move(ref_packet);
                    result = std::move(response);
                    keep_trying = false;
                    break;
                }
                default:
                {
                    CommanderMessage::Failed response;
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
                               &index_rewards
                               ]()
    {
        log_index.discard();
        rpc_server.head_block_index.discard();
        rpc_server.accounts.discard();
        rpc_server.blocks.discard();
        rpc_server.storages.discard();
        rpc_server.channels.discard();

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
        socket.send(peerid, beltpp::packet(req));

        size_t count = 0;
        bool new_import_done = false;

        while (true)
        {
            unordered_set<beltpp::ievent_item const*> wait_sockets;
            auto wait_result = eh.wait(wait_sockets);
            B_UNUSED(wait_sockets);

            if (wait_result & beltpp::event_handler::event)
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

                                    uint64_t block_index = head_block_index();

                                    if (!new_import)
                                    {
                                        CommanderMessage::BlockInfo block_info;

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
                                        ++count;

                                        process_storage_tansactions(set_accounts,
                                                                    transaction_log,
                                                                    rpc_server,
                                                                    LoggingType::apply);

                                        process_channel_tansactions(set_accounts,
                                                                    transaction_log,
                                                                    rpc_server,
                                                                    LoggingType::apply);

                                        process_statistics_tansactions(transaction_log,
                                                                       rpc_server,
                                                                       LoggingType::apply);

                                        update_balances(set_accounts,
                                                        rpc_server,
                                                        transaction_log,
                                                        block_log.authority,
                                                        LoggingType::apply);

                                        process_transactions(block_index,
                                                             transaction_log,
                                                             set_accounts,
                                                             transactions,
                                                             index_transactions,
                                                             block_log.authority,
                                                             LoggingType::apply);
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

                                    process_storage_tansactions(set_accounts,
                                                                transaction_log,
                                                                rpc_server,
                                                                LoggingType::apply);

                                    process_channel_tansactions(set_accounts,
                                                                transaction_log,
                                                                rpc_server,
                                                                LoggingType::apply);

                                    update_balances(set_accounts,
                                                    rpc_server,
                                                    transaction_log,
                                                    string(),
                                                    LoggingType::apply);

                                    process_transactions(block_index,
                                                         transaction_log,
                                                         set_accounts,
                                                         transactions,
                                                         index_transactions,
                                                         string(),
                                                         LoggingType::apply);
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

                                        process_storage_tansactions(set_accounts,
                                                                    transaction_log,
                                                                    rpc_server,
                                                                    LoggingType::revert);

                                        process_channel_tansactions(set_accounts,
                                                                    transaction_log,
                                                                    rpc_server,
                                                                    LoggingType::revert);

                                        process_statistics_tansactions(transaction_log,
                                                                       rpc_server,
                                                                       LoggingType::revert);

                                        update_balances(set_accounts,
                                                        rpc_server,
                                                        transaction_log,
                                                        block_log.authority,
                                                        LoggingType::revert);

                                        process_transactions(block_index,
                                                             transaction_log,
                                                             set_accounts,
                                                             transactions,
                                                             index_transactions,
                                                             block_log.authority,
                                                             LoggingType::revert);
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

                                    if (!new_import)
                                    {
                                        rpc_server.blocks.pop_back();
                                    }
                                }
                                else if (action_type == TransactionLog::rtt)
                                {
                                    TransactionLog transaction_log;
                                    std::move(action_info.action).get(transaction_log);

                                    uint64_t block_index = head_block_index() + 1;

                                    process_storage_tansactions(set_accounts,
                                                                transaction_log,
                                                                rpc_server,
                                                                LoggingType::revert);

                                    process_channel_tansactions(set_accounts,
                                                                transaction_log,
                                                                rpc_server,
                                                                LoggingType::revert);

                                    update_balances(set_accounts,
                                                    rpc_server,
                                                    transaction_log,
                                                    string(),
                                                    LoggingType::revert);

                                    process_transactions(block_index,
                                                         transaction_log,
                                                         set_accounts,
                                                         transactions,
                                                         index_transactions,
                                                         string(),
                                                         LoggingType::revert);
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
    rpc_server.storages.save();
    rpc_server.channels.save();
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
    rpc_server.storages.commit();
    rpc_server.channels.commit();
    log_index.commit();

    for (auto& tr : transactions)
        tr.second.commit();
    for (auto& rw : rewards)
        rw.second.save();
    for (auto& tr : index_transactions)
        tr.second.commit();
    for (auto& rw : index_rewards)
        rw.second.commit();
}
