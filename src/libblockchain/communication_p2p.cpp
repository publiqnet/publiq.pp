#include "communication_p2p.hpp"
#include "communication_rpc.hpp"
#include "transaction_handler.hpp"

#include "coin.hpp"
#include "common.hpp"
#include "sessions.hpp"
#include "exception.hpp"
#include "message.tmpl.hpp"
#include "types.hpp"

#include <mesh.pp/cryptoutility.hpp>

#include <algorithm>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <chrono>

using namespace BlockchainMessage;

using std::map;
using std::set;
using std::multimap;
using std::unordered_map;
using std::unordered_set;
using std::make_pair;
namespace chrono = std::chrono;
using chrono::system_clock;

namespace publiqpp
{
bool apply_transaction(SignedTransaction const& signed_transaction,
                       publiqpp::detail::node_internals& impl,
                       string const& key/* = string()*/)
{
    state_layer layer = state_layer::pool;
    if (false == key.empty())
        layer = state_layer::chain;

    if (false == action_can_apply(impl,
                                  signed_transaction,
                                  signed_transaction.transaction_details.action,
                                  layer))
        return false;


    action_apply(impl,
                 signed_transaction,
                 signed_transaction.transaction_details.action,
                 layer);

    if (false == fee_can_apply(impl, signed_transaction))
    {
        action_revert(impl,
                      signed_transaction,
                      signed_transaction.transaction_details.action,
                      layer);
        return false;
    }

    fee_apply(impl, signed_transaction, key);

    return true;
}

void revert_transaction(SignedTransaction const& signed_transaction,
                        publiqpp::detail::node_internals& impl,
                        string const& key/* = string()*/)
{
    fee_revert(impl, signed_transaction, key);

    state_layer layer = state_layer::pool;
    if (false == key.empty())
        layer = state_layer::chain;

    action_revert(impl,
                  signed_transaction,
                  signed_transaction.transaction_details.action,
                  layer);
}

bool stat_mismatch(uint64_t first, uint64_t second)
{
    return std::max(first, second) > std::min(first, second) * STAT_ERROR_LIMIT;
}

void validate_statistics(map<string, ServiceStatistics> const& channel_provided_statistics,
                         map<string, ServiceStatistics> const& storage_provided_statistics,
                         multimap<string, pair<uint64_t, uint64_t>>& author_result,
                         multimap<string, pair<uint64_t, uint64_t>>& channel_result,
                         multimap<string, pair<uint64_t, uint64_t>>& storage_result,
                         //  uri         channel   views
                         map<string, map<string, uint64_t>>& map_unit_uri_view_counts,
                         uint64_t block_number,
                         publiqpp::detail::node_internals& impl)
{
    author_result.clear();
    channel_result.clear();
    storage_result.clear();

    map_unit_uri_view_counts.clear();

    map<string, map<string, map<string, uint64_t>>> channel_statistics;
    map<string, map<string, set<string>>> cross_verified_statistics;

    unordered_set<string> file_uris, unit_uris;

    // group channel provided data to verify in comming steps
    for (auto const& stat_item : channel_provided_statistics)
    for (auto const& file_item : stat_item.second.file_items)
    for (auto const& count_item : file_item.count_items)
    {
        file_uris.insert(file_item.file_uri);
        unit_uris.insert(file_item.unit_uri);

        string channel_id = stat_item.first;
        string file_uri = file_item.file_uri;
        string storage_id = count_item.peer_address;

        assert(count_item.count != 0);
        if (count_item.count == 0)
            throw std::logic_error("count_item.count == 0");

        // it is safe to assume that initial value 0 will be created
        channel_statistics[channel_id][file_uri][storage_id] += count_item.count;
    }

    // cross compare channel and storage provided data
    for (auto const& stat_item : storage_provided_statistics)
    for (auto const& file_item : stat_item.second.file_items)
    for (auto const& count_item : file_item.count_items)
    {
        string channel_id = count_item.peer_address;
        string file_uri = file_item.file_uri;
        string storage_id = stat_item.first;

        // channel_statistics is not used anymore so
        // don't care if 0 value is created below, when it dit not exist
        uint64_t stat_value = channel_statistics[channel_id][file_uri][storage_id];

        assert(count_item.count != 0);
        if (count_item.count == 0)
            throw std::logic_error("count_item.count == 0");

        if (stat_value > 0 &&
            false == stat_mismatch(stat_value, count_item.count))
            cross_verified_statistics[channel_id][file_uri].insert(storage_id);
    }

    // from here on - cross_verified_statistics holds values only for agreeing entries
    // storage_provided_statistics and channel_statistics are not used

    auto check_file_uris = impl.m_documents.files_exist(file_uris);
    assert(check_file_uris.first);
    if (false == check_file_uris.first)
        throw std::logic_error("false == check_file_uris.first");
    auto check_unit_uris = impl.m_documents.units_exist(unit_uris);
    assert(check_unit_uris.first);
    if (false == check_unit_uris.first)
        throw std::logic_error("false == check_unit_uris.first");

    // group views by storage
    // sum by serving channel, unit and file
    // storage     view
    map<string, uint64_t> storage_group;

    // group views by unit and file
    // sum by serving channel and storage
    // unit_uri    file_uri   view
    map<string, map<string, uint64_t>> author_group;

    // group views by serving channel, owner channel
    // group by content id, file_uri and unit (units are limited by the particular content_id)
    // sum by storage
    // will take max by file and unit later then sum again by content id
    // channel     owner       content_id   unit_uri     file_uri   view
    map<string, map<string, map<uint64_t, map<string, map<string, uint64_t>>>>> content_group;

    for (auto const& stat_item : channel_provided_statistics)
    for (auto const& file_item : stat_item.second.file_items)
    for (auto const& count_item : file_item.count_items)
    {
        string channel_id = stat_item.first;
        string file_uri = file_item.file_uri;
        string unit_uri = file_item.unit_uri;
        string storage_id = count_item.peer_address;
        uint64_t view_count = count_item.count;

        // cross_verified_statistics will not be used anymore
        // so, don't care if empty object gets created
        bool is_cross_verified = cross_verified_statistics[channel_id][file_uri].count(storage_id);

        assert(view_count > 0);
        if (0 == view_count)
            throw std::logic_error("0 == view_count");

        if (is_cross_verified)
        {
            storage_group[storage_id] += view_count;
            author_group[unit_uri][file_uri] += view_count;

            ContentUnit content_unit = impl.m_documents.get_unit(unit_uri);
            content_group[channel_id][content_unit.channel_address][content_unit.content_id][unit_uri][file_uri] += view_count;
        }
    }

    uint64_t total_view_all_files_count = 0;
    // collect storages final result
    // the sum of values hold by storage_result is the total_view_units_count
    for (auto const& item : storage_group)
    {
        total_view_all_files_count += item.second;
        storage_result.insert({item.first, {item.second, 1}});
    }

    for (auto& item_result : storage_result)
        item_result.second.second *= total_view_all_files_count;

    uint64_t total_view_units_count = 0;
    // collect authors final result
    for (auto const& item_per_unit : author_group)
    {
        uint64_t total = 0;
        uint64_t file_count = item_per_unit.second.size();
        for (auto const& item_per_file : item_per_unit.second)
            total += item_per_file.second;

        assert(file_count);
        if (0 == file_count)
            throw std::logic_error("0 == file_count");

        // get average value as a unit usage
        total /= file_count;

        total_view_units_count += total;

        assert(total != 0);
        if (0 == total)
            throw std::logic_error("0 == total");

        for (auto const& item_per_file : item_per_unit.second)
        {
            string const& file_uri = item_per_file.first;

            File file = impl.m_documents.get_file(file_uri);
            uint64_t authors_count = file.author_addresses.size();

            for (auto const& author_address : file.author_addresses)
                author_result.insert({author_address, {total, file_count * authors_count}});
        }
    }

    for (auto& item_result : author_result)
        item_result.second.second *= total_view_units_count;

    uint64_t total_channel_view_count = 0;
    // collect channels final result
    for (auto const& item_per_server : content_group)
    {
        // item_per_server.first is the serving channel
        string const& serving_channel = item_per_server.first;

        for (auto const& item_per_owner : item_per_server.second)
        {
            // item_per_owner.first is the owner channel
            string const& owner_channel = item_per_owner.first;

            for (auto const& item_per_content_id : item_per_owner.second)
            {
                for (auto const& item_per_unit : item_per_content_id.second)
                {
                    auto& unit_value = map_unit_uri_view_counts[item_per_unit.first][serving_channel];

                    for (auto const& item_per_file : item_per_unit.second)
                        unit_value = std::max(unit_value, item_per_file.second);
                }
            }

            /*uint64_t count = 0;
            for (auto const& item_per_content_id : item_per_owner.second)
            {
                uint64_t max_count_per_content_id = 0;
                for (auto const& item_per_unit : item_per_content_id.second)
                for (auto const& item_per_file : item_per_unit.second)
                    max_count_per_content_id = std::max(max_count_per_content_id, item_per_file.second);
                count += max_count_per_content_id;
            }*/
            uint64_t count = impl.pcounts_per_channel_views(item_per_owner.second,
                                                            block_number,
                                                            impl.m_testnet);

            if (serving_channel == owner_channel)
            {
                channel_result.insert({serving_channel, {2 * count, 2}});
            }
            else
            {
                channel_result.insert({owner_channel, {count, 2}});
                channel_result.insert({serving_channel, {count, 2}});
            }

            total_channel_view_count += count;
        }
    }

    for (auto& item_result : channel_result)
        item_result.second.second *= total_channel_view_count;
}

coin distribute_rewards(vector<Reward>& rewards,
                        multimap<string, pair<uint64_t, uint64_t>> const& stat_distribution,
                        coin total_amount,
                        RewardType reward_type)
{
    // total_amount will go to miner if no stat distribution
    if (stat_distribution.size() == 0 || total_amount.empty())
        return total_amount;

#if 0
    double stat_dist_sum = 0;
    for (auto const& temp : stat_distribution)
        stat_dist_sum += double(temp.second.first) / double(temp.second.second);
    //  this is for testing purposes. sum must be 1
#endif

    coin rest_amount = total_amount;
    map<string, coin> coin_distribution;
    for (auto const& item : stat_distribution)
    {
        coin amount = (total_amount * item.second.first) / item.second.second;

        rest_amount -= amount;
        coin_distribution[item.first] += amount;
    }

    Reward reward;
    reward.reward_type = reward_type;

    for (auto const& item : coin_distribution)
    {
        reward.to = item.first;
        item.second.to_Coin(reward.amount);

        rewards.push_back(reward);
    }

    // rounding error fix
    if(rest_amount > coin(0,0))
        (rest_amount + rewards.back().amount).to_Coin(rewards.back().amount);

    return coin();
}

void grant_rewards(vector<SignedTransaction> const& signed_transactions,
                   vector<Reward>& rewards,
                   string const& address,
                   BlockHeader const& block_header,
                   rewards_type type,
                   publiqpp::detail::node_internals& impl,
                   //  uri         channel   views
                   map<string, map<string, uint64_t>>& unit_uri_view_counts,
                   //  sp.txid applied
                   map<string, coin>& applied_sponsor_items)
{
    rewards.clear();
    unit_uri_view_counts.clear();
    applied_sponsor_items.clear();

    map<string, ServiceStatistics> channel_provided_statistics;
    map<string, ServiceStatistics> storage_provided_statistics;

    map<string, coin> sponsored_rewards_returns;

    for (auto it = signed_transactions.begin(); it != signed_transactions.end(); ++it)
    {
        // only servicestatistics corresponding to current block will be taken
        if (it->transaction_details.action.type() == ServiceStatistics::rtt)
        {
            ServiceStatistics const* service_statistics;
            it->transaction_details.action.get(service_statistics);

            NodeType node_type;
            if (impl.m_state.get_role(service_statistics->server_address, node_type))
            {
                assert(node_type == NodeType::channel || node_type == NodeType::storage);
                if (node_type != NodeType::channel && node_type != NodeType::storage)
                    throw std::logic_error("node_type != NodeType::channel && node_type != NodeType::storage");

                map<string, ServiceStatistics>* pstatistics = nullptr;
                if (node_type == NodeType::channel)
                    pstatistics = &channel_provided_statistics;
                else if (node_type == NodeType::storage)
                    pstatistics = &storage_provided_statistics;

                auto insert_result = pstatistics->insert({
                                                             service_statistics->server_address,
                                                             *service_statistics
                                                         });

                //  unfortunately there is already a block with double stat reports
                //  so cannot go on with the below code
                /*
                if (false == insert_result.second)
                    throw wrong_data_exception("statistics from " +
                                               service_statistics->server_address +
                                               " are already included in the block");
                                               */
                //  keep the last statistics report - overwrite the original value
                if (false == insert_result.second)
                    insert_result.first->second = *service_statistics;
            }
        }
        else if (it->transaction_details.action.type() == CancelSponsorContentUnit::rtt)
        {
            CancelSponsorContentUnit const* cancel_sponsor_content_unit;
            it->transaction_details.action.get(cancel_sponsor_content_unit);

            auto const& expiry_entry =
                    impl.m_documents.expiration_entry_ref_by_hash(cancel_sponsor_content_unit->transaction_hash);

            assert(expiry_entry.manually_cancelled != StorageTypes::Coin());
            if (expiry_entry.manually_cancelled == StorageTypes::Coin())
                throw std::logic_error("expiry_entry.manually_cancelled == StorageTypes::Coin()");

            auto& sponsored_reward_ref = sponsored_rewards_returns[cancel_sponsor_content_unit->sponsor_address];
            sponsored_reward_ref += coin(expiry_entry.manually_cancelled);
        }
    }

    multimap<string, pair<uint64_t, uint64_t>> author_result;
    multimap<string, pair<uint64_t, uint64_t>> channel_result;
    multimap<string, pair<uint64_t, uint64_t>> storage_result;

    validate_statistics(channel_provided_statistics,
                        storage_provided_statistics,
                        author_result,
                        channel_result,
                        storage_result,
                        unit_uri_view_counts,
                        block_header.block_number,
                        impl);

    assert(unit_uri_view_counts.empty() || (false == unit_uri_view_counts.empty() &&
                                            false == author_result.empty() &&
                                            false == channel_result.empty() &&
                                            false == storage_result.empty()));

    if(false == unit_uri_view_counts.empty() && (author_result.empty() || channel_result.empty() || storage_result.empty()))
        throw std::logic_error("wrong result from validate_statistics(...)");

    size_t year_index = block_header.block_number / 50000;
    coin miner_emission_reward, author_emission_reward, channel_emission_reward, storage_emission_reward;
    coin channel_sponsored_reward, storage_sponsored_reward, author_sponsored_reward;

    coin sponsored_reward = coin(0, 0);

    for (auto const& unit_uri : unit_uri_view_counts)
    {
        // sponsor       txid   amount
        map<string, map<string, coin>> sponsored_rewards =
        impl.m_documents.sponsored_content_unit_set_used(impl,
                                                         unit_uri.first,
                                                         block_header.block_number,
                                                         rewards_type::apply == type ?
                                                             documents::sponsored_content_unit_set_used_apply :
                                                             documents::sponsored_content_unit_set_used_revert,
                                                         string(),  //  transaction_hash_to_validate
                                                         string(),  //  manual_by_account
                                                         false);
        for (auto const& sponsored_reward_by_sponsor : sponsored_rewards)
        for (auto const& sponsored_reward_by_sponsor_by_txid : sponsored_reward_by_sponsor.second)
        {
            sponsored_reward += sponsored_reward_by_sponsor_by_txid.second;

            auto insert_result =
                    applied_sponsor_items.insert({
                                                     sponsored_reward_by_sponsor_by_txid.first, // txid
                                                     sponsored_reward_by_sponsor_by_txid.second // coin
                                                 });

            if (false == insert_result.second)
                throw std::logic_error("applied_sponsor_items.insert");
        }
    }

    auto expirings = impl.m_documents.content_unit_uri_sponsor_expiring(block_header.block_number);
    for (auto const& expiring_item : expirings)
    {
        auto const& expiring_item_uri = expiring_item.first;
        auto const& expiring_item_transaction_hash = expiring_item.second;
        if (0 == unit_uri_view_counts.count(expiring_item_uri))
        {
            // author       txid   amount
            map<string, map<string, coin>> temp_sponsored_rewards =
                                impl.m_documents.sponsored_content_unit_set_used(impl,
                                                                                 expiring_item_uri,
                                                                                 block_header.block_number,
                                                                                 rewards_type::apply == type ?
                                                                                     documents::sponsored_content_unit_set_used_apply :
                                                                                     documents::sponsored_content_unit_set_used_revert,
                                                                                 expiring_item_transaction_hash,
                                                                                 string(),  //  manual_by_account
                                                                                 false);

            for (auto const& sponsored_reward_by_sponsor : temp_sponsored_rewards)
            {
                auto& sponsored_reward_ref = sponsored_rewards_returns[sponsored_reward_by_sponsor.first];

                for(auto const& sponsored_reward_by_sponsor_by_txid : sponsored_reward_by_sponsor.second)
                {
                    assert(sponsored_reward_by_sponsor_by_txid.second != coin());
                    if (sponsored_reward_by_sponsor_by_txid.second == coin())
                        throw std::logic_error("sponsored_reward_by_sponsor_by_txid.second == coin()");

                    sponsored_reward_ref += sponsored_reward_by_sponsor_by_txid.second;
                }
            }
        }
    }

    coin emission_reward = impl.m_block_reward_array[year_index];

    if (year_index < impl.m_block_reward_array.size())
    {
        miner_emission_reward = emission_reward * MINER_EMISSION_REWARD_PERCENT / 100;
        author_emission_reward = emission_reward * AUTHOR_EMISSION_REWARD_PERCENT / 100;
        channel_emission_reward = emission_reward * CHANNEL_EMISSION_REWARD_PERCENT / 100;
        storage_emission_reward = emission_reward - miner_emission_reward - author_emission_reward - channel_emission_reward;
    }

    author_sponsored_reward = sponsored_reward * AUTHOR_SPONSORED_REWARD_PERCENT / 100;
    channel_sponsored_reward = sponsored_reward * CHANNEL_SPONSORED_REWARD_PERCENT / 100;
    storage_sponsored_reward = sponsored_reward - author_sponsored_reward - channel_sponsored_reward;

    // grant rewards to authors, channels and storages
    miner_emission_reward += distribute_rewards(rewards, author_result, author_emission_reward + author_sponsored_reward, RewardType::author);
    miner_emission_reward += distribute_rewards(rewards, channel_result, channel_emission_reward + channel_sponsored_reward, RewardType::channel);
    miner_emission_reward += distribute_rewards(rewards, storage_result, storage_emission_reward + storage_sponsored_reward, RewardType::storage);

    // if sponsored items expired without service or have been cancelled
    // reward the coins back to sponsor
    for (auto const& sponsored_reward_ref : sponsored_rewards_returns)
    {
        Reward reward;
        reward.to = sponsored_reward_ref.first;
        sponsored_reward_ref.second.to_Coin(reward.amount);
        reward.reward_type = RewardType::sponsored_return;

        rewards.push_back(reward);
    }

    // grant miner reward himself
    if (!miner_emission_reward.empty())
    {
        Reward reward;
        reward.to = address;
        miner_emission_reward.to_Coin(reward.amount);
        reward.reward_type = RewardType::miner;

        rewards.push_back(reward);
    }
}

//  this has opposite bool logic - true means error :)
bool check_headers(BlockHeaderExtended const& next_header, BlockHeaderExtended const& header)
{
    bool t = next_header.block_number != header.block_number + 1;
    t = t || next_header.c_sum <= header.c_sum;
    t = t || next_header.c_sum != next_header.delta + header.c_sum;
    t = t || (next_header.c_const != header.c_const &&
              next_header.c_const != header.c_const * 2 &&
              next_header.c_const != header.c_const / 2);
    t = t || (next_header.prev_hash != header.block_hash);

    system_clock::time_point time_point1 = system_clock::from_time_t(header.time_signed.tm);
    system_clock::time_point time_point2 = system_clock::from_time_t(next_header.time_signed.tm);
    chrono::seconds diff_seconds = chrono::duration_cast<chrono::seconds>(time_point2 - time_point1);

    return t || time_point1 > time_point2 || diff_seconds.count() != BLOCK_MINE_DELAY;
}

bool check_rewards(Block const& block,
                   string const& authority,
                   rewards_type type,
                   publiqpp::detail::node_internals& impl,
                   //  uri         channel   views
                   map<string, map<string, uint64_t>>& unit_uri_view_counts,
                   //  sp.txid applied
                   map<string, coin>& applied_sponsor_items)
{
    vector<Reward> rewards;
    grant_rewards(block.signed_transactions,
                  rewards,
                  authority,
                  block.header,
                  type,
                  impl,
                  unit_uri_view_counts,
                  applied_sponsor_items);

    auto it1 = rewards.begin();
    auto it2 = block.rewards.begin();

    bool bad_reward = rewards.size() != block.rewards.size();

    while (!bad_reward && it1 != rewards.end())
    {
        bad_reward = *it1 != *it2;

        ++it1;
        ++it2;
    }

    return bad_reward;
}

bool check_service_statistics(Block const& block,
                              vector<SignedTransaction> const& pool_transactions,
                              vector<SignedTransaction> const& reverted_transactions,
                              publiqpp::detail::node_internals& impl)
{
    size_t block_channel_stat_count = 0;
    size_t block_storage_stat_count = 0;
    size_t known_channel_stat_count = 0;
    size_t known_storage_stat_count = 0;

    auto tp_end = system_clock::from_time_t(block.header.time_signed.tm) - chrono::seconds(BLOCK_MINE_DELAY);
    auto tp_start = system_clock::from_time_t(block.header.time_signed.tm) - chrono::seconds(2 * BLOCK_MINE_DELAY);

    for (auto it = block.signed_transactions.begin(); it != block.signed_transactions.end(); ++it)
    {
        if (it->transaction_details.action.type() == ServiceStatistics::rtt)
        {
            ServiceStatistics const* service_statistics;
            it->transaction_details.action.get(service_statistics);

            if (service_statistics->start_time_point.tm == system_clock::to_time_t(tp_start) &&
                service_statistics->end_time_point.tm == system_clock::to_time_t(tp_end))
            {
                NodeType node_type;
                if (impl.m_state.get_role(service_statistics->server_address, node_type))
                {
                    if (node_type == NodeType::channel)
                        ++block_channel_stat_count;
                    else
                        ++block_storage_stat_count;
                }
            }
        }
    }

    for (auto it = pool_transactions.begin(); it != pool_transactions.end(); ++it)
    {
        if (it->transaction_details.action.type() == ServiceStatistics::rtt)
        {
            ServiceStatistics const* service_statistics;
            it->transaction_details.action.get(service_statistics);

            if (service_statistics->start_time_point.tm == system_clock::to_time_t(tp_start) &&
                service_statistics->end_time_point.tm == system_clock::to_time_t(tp_end))
            {
                NodeType node_type;
                if (impl.m_state.get_role(service_statistics->server_address, node_type))
                {
                    if (node_type == NodeType::channel)
                        ++known_channel_stat_count;
                    else
                        ++known_storage_stat_count;
                }
            }
        }
    }

    for (auto it = reverted_transactions.begin(); it != reverted_transactions.end(); ++it)
    {
        if (it->transaction_details.action.type() == ServiceStatistics::rtt)
        {
            ServiceStatistics const* service_statistics;
            it->transaction_details.action.get(service_statistics);

            if (service_statistics->start_time_point.tm == system_clock::to_time_t(tp_start) &&
                service_statistics->end_time_point.tm == system_clock::to_time_t(tp_end))
            {
                NodeType node_type;
                if (impl.m_state.get_role(service_statistics->server_address, node_type))
                {
                    if (node_type == NodeType::channel)
                        ++known_channel_stat_count;
                    else
                        ++known_storage_stat_count;
                }
            }
        }
    }

    // at least 50% known acceptable service statistics must be included
    // in current block for channels and storages seperately

    return 2 * block_channel_stat_count < known_channel_stat_count ||
           2 * block_storage_stat_count < known_storage_stat_count;
}

uint64_t check_delta_vector(vector<pair<uint64_t, uint64_t>> const& delta_vector, std::string& error)
{
    static_assert(DELTA_STEP > 0, "check this please");
    assert(false == delta_vector.empty());
    if (delta_vector.empty())
        throw std::logic_error("check_delta_vector(): delta_vector.empty()");

    uint64_t expected_c_const;
    if (delta_vector.size() >= DELTA_STEP)
        expected_c_const = delta_vector[DELTA_STEP - 1].second;
    else
        expected_c_const = delta_vector.back().second;

    size_t check_c_const_at_index = DELTA_STEP;

    auto set_error = [&error](size_t idx, uint64_t actual, uint64_t expected)
    {
        error = "c_const at index " + std::to_string(idx) +
                " is expected to be " + std::to_string(expected) +
                " instead of " + std::to_string(actual);
        return expected;
    };

    size_t index_end = 0;
    if (delta_vector.size() >= DELTA_STEP)
        index_end = delta_vector.size() - DELTA_STEP + 1;
    else
        index_end = 1;

    for (size_t index = 0; index != index_end; ++index)
    {
        bool have_enough_steps = true;

        uint64_t delta_sum = 0;
        size_t index2_end = index + DELTA_STEP;
        if (index2_end > delta_vector.size())
        {
            index2_end = delta_vector.size();
            have_enough_steps = false;
        }

        for (size_t index2 = index; index2 != index2_end; ++index2)
        {
            auto const& ref_delta = delta_vector[index2].first;
            auto const& ref_c_const = delta_vector[index2].second;

            if (check_c_const_at_index == index2)
            {
                ++check_c_const_at_index;

                if (ref_c_const != expected_c_const)
                    return set_error(index2, ref_c_const, expected_c_const);
            }

            delta_sum += ref_delta;
        }

        if (have_enough_steps &&
            delta_sum > DELTA_STEP * DELTA_UP)
            expected_c_const *= 2;
        else if (have_enough_steps &&
                 delta_sum < DELTA_STEP * DELTA_DOWN &&
                 expected_c_const > 1)
            expected_c_const /= 2;
    }

    return expected_c_const;
}

vector<SignedTransaction>
revert_pool(time_t expiry_time, publiqpp::detail::node_internals& impl)
{
    vector<SignedTransaction> pool_transactions;

    //  collect transactions to be reverted from pool
    //
    size_t state_pool_size = impl.m_transaction_pool.length();

    for (size_t index = 0; index != state_pool_size; ++index)
    {
        SignedTransaction const& signed_transaction = impl.m_transaction_pool.at(index);

        if (expiry_time <= signed_transaction.transaction_details.expiry.tm)
        {
            pool_transactions.push_back(signed_transaction);
        }
    }

    //  revert transactions from pool
    //
    for (size_t index = state_pool_size; index != 0; --index)
    {
        SignedTransaction& ref_signed_transaction = impl.m_transaction_pool.ref_at(index - 1);
        SignedTransaction signed_transaction = std::move(ref_signed_transaction);

        impl.m_transaction_pool.pop_back();
        bool complete = impl.m_transaction_cache.erase_pool(signed_transaction);

        if (complete)
        {
            impl.m_action_log.revert();
            revert_transaction(signed_transaction, impl);
        }
    }

    assert(impl.m_transaction_pool.length() == 0);

    //  sync the node balance at chain level
    impl.m_state.set_balance(impl.m_pb_key.to_string(),
                             impl.m_state.get_balance(impl.m_pb_key.to_string(), state_layer::pool),
                             state_layer::chain);

    return pool_transactions;
}

void mine_block(publiqpp::detail::node_internals& impl)
{
    impl.m_transaction_cache.backup();
    beltpp::on_failure guard([&impl]
    {
        impl.m_storage_controller.discard();
        impl.discard();
        impl.m_transaction_cache.restore();
    });

    uint64_t block_number = impl.m_blockchain.length() - 1;

    //  calculate consensus_const
    vector<pair<uint64_t, uint64_t>> delta_vector;
    size_t block_index = 0;
    if (block_number + 1 >= DELTA_STEP)
        block_index = block_number + 1 - DELTA_STEP;
    for (; block_index <= block_number; ++block_index)
    {
        BlockHeader const& tmp_header = impl.m_blockchain.header_at(block_index);
        delta_vector.push_back(std::make_pair(tmp_header.delta, tmp_header.c_const));
    }

    assert(false == delta_vector.empty());

    string check_delta_vector_error;
    uint64_t calculate_c_const = check_delta_vector(delta_vector, check_delta_vector_error);
    assert(check_delta_vector_error.empty());
    if (false == check_delta_vector_error.empty())
        throw std::logic_error("own blockchain is somehow wrong");

    SignedBlock const& prev_signed_block = impl.m_blockchain.at(block_number);
    BlockHeader const& prev_header = impl.m_blockchain.last_header();
    string own_key = impl.m_pb_key.to_string();
    string prev_hash = meshpp::hash(prev_signed_block.block_details.to_string());

    uint64_t delta = impl.calc_delta(own_key,
                                     impl.get_balance().whole,
                                     prev_hash,
                                     prev_header.c_const);

    // fill new block header data
    BlockHeader block_header;
    block_header.block_number = block_number + 1;
    block_header.delta = delta;
    block_header.c_const = calculate_c_const;
    block_header.c_sum = prev_header.c_sum + delta;
    block_header.prev_hash = prev_hash;
    block_header.time_signed.tm = prev_header.time_signed.tm + BLOCK_MINE_DELAY;

    Block block;
    block.header = block_header;

    vector<SignedTransaction> pool_transactions;
    vector<SignedTransaction> block_transactions;
    vector<SignedTransaction> channel_statistics;
    vector<SignedTransaction> storage_statistics;
    vector<SignedTransaction> reverted_transactions;

    auto tp_end = system_clock::from_time_t(prev_header.time_signed.tm);
    auto tp_start = tp_end - chrono::seconds(BLOCK_MINE_DELAY);

    //  collect transactions to be reverted from pool
    //  revert transactions from pool
    reverted_transactions = revert_pool(block_header.time_signed.tm, impl);

    auto reverted_transactions_it_end =
            std::remove_if(reverted_transactions.begin(), reverted_transactions.end(),
                           [&block_header,
                           &pool_transactions](SignedTransaction& signed_transaction)
    {
        if (signed_transaction.transaction_details.creation >= block_header.time_signed)
        {
            pool_transactions.push_back(std::move(signed_transaction));
            return true;
        }
        return false;
    });
    reverted_transactions.erase(reverted_transactions_it_end, reverted_transactions.end());

    reverted_transactions_it_end =
            std::remove_if(reverted_transactions.begin(), reverted_transactions.end(),
                           [&tp_start,
                            &tp_end,
                            &impl,
                            &channel_statistics,
                            &storage_statistics](SignedTransaction& signed_tr)
    {
        if (signed_tr.transaction_details.action.type() == ServiceStatistics::rtt)
        {
            ServiceStatistics* service_statistics;
            signed_tr.transaction_details.action.get(service_statistics);

            if (service_statistics->start_time_point.tm == system_clock::to_time_t(tp_start) &&
                service_statistics->end_time_point.tm == system_clock::to_time_t(tp_end))
            {
                NodeType node_type;
                if (impl.m_state.get_role(service_statistics->server_address, node_type))
                {
                    if (node_type == NodeType::channel)
                        channel_statistics.push_back(std::move(signed_tr));
                    else
                        storage_statistics.push_back(std::move(signed_tr));

                    return true;
                }
            }
        }

        return false;
    });
    reverted_transactions.erase(reverted_transactions_it_end, reverted_transactions.end());

    auto reserve_statistics = [&block_transactions, &reverted_transactions](vector<SignedTransaction>& statistics)
    {
        std::sort(statistics.begin(), statistics.end(),
            [](SignedTransaction const& lhs, SignedTransaction const& rhs)
        {
            return coin(lhs.transaction_details.fee) > coin(rhs.transaction_details.fee);
        });

        size_t stat_index = 0;
        size_t stat_count = statistics.size();

        for (auto& stat : statistics)
        {
            if (2 * stat_index < stat_count + 1 &&
                block_transactions.size() < size_t(BLOCK_MAX_TRANSACTIONS))
            {
                ++stat_index;
                block_transactions.push_back(std::move(stat));
            }
            else
            {
                reverted_transactions.push_back(std::move(stat));
            }
        }
    };

    reserve_statistics(channel_statistics);
    reserve_statistics(storage_statistics);

    //  here we collect incomplete transactions and try to find
    //  if they form a complete transaction already
    unordered_map<string, vector<SignedTransaction>> map_incomplete_transactions;

    reverted_transactions_it_end =
            std::remove_if(reverted_transactions.begin(), reverted_transactions.end(),
                           [&impl,
                           &map_incomplete_transactions](SignedTransaction& signed_transaction)
    {
        if (false == action_is_complete(impl, signed_transaction))
        {
            string incomplete_key = signed_transaction.transaction_details.to_string();
            vector<SignedTransaction>& transactions = map_incomplete_transactions[incomplete_key];

            transactions.emplace_back(std::move(signed_transaction));
            return true;
        }
        return false;
    });
    reverted_transactions.erase(reverted_transactions_it_end, reverted_transactions.end());


    for (auto& key_stxs : map_incomplete_transactions)
    {
        unordered_map<string, string> map_authorizations;

        auto const& stxs = key_stxs.second;
        assert(false == stxs.empty());

        for (auto const& stx : stxs)
        {
            assert(stx.authorizations.size() == 1);
            map_authorizations[stx.authorizations.front().address] = stx.authorizations.front().signature;
        }

        auto signed_transaction = stxs.front();
        signed_transaction.authorizations.clear();

        vector<string> owners = action_owners(signed_transaction);

        bool not_found = false;
        for (auto const& owner : owners)
        {
            auto it_map = map_authorizations.find(owner);
            if (map_authorizations.end() != it_map)
            {
                Authority temp_authority;
                temp_authority.address = it_map->first;
                temp_authority.signature = it_map->second;

                signed_transaction.authorizations.push_back(std::move(temp_authority));
            }
            else
            {
                not_found = true;
                break;
            }
        }

        if (false == not_found)
        {
            reverted_transactions.push_back(std::move(signed_transaction));
        }
        else
        {
            for (auto& stx : stxs)
            {
                pool_transactions.push_back(std::move(stx));
            }
        }
    }

    struct extented_info
    {
        coin value;
        size_t size;
        SignedTransaction stx;
    };

    unordered_map<string, unordered_set<size_t>> index_participants;
    vector<extented_info> reverted_transactions_ex;
    reverted_transactions_ex.reserve(reverted_transactions.size());
    for (size_t index = 0; index != reverted_transactions.size(); ++index)
    {
        auto& reverted_transaction = reverted_transactions[index];
        vector<string> participants = action_participants(reverted_transaction);
        for (auto const& participant : participants)
        {
            if (false == participant.empty())
            {
                index_participants[participant].insert(index);
            }
        }

        extented_info temp;
        temp.size = 0;
        temp.stx = std::move(reverted_transaction);
        reverted_transactions_ex.push_back(std::move(temp));
    }
    reverted_transactions.clear();

    for (size_t index = 0; index != reverted_transactions_ex.size(); ++index)
    {
        coin& value = reverted_transactions_ex[index].value;
        size_t& size = reverted_transactions_ex[index].size;

        // skip already calculated transactions
        if (size > 0) continue;

        unordered_set<size_t> used_indices;
        unordered_set<size_t> next_indices = { index };

        while (false == next_indices.empty())
        {
            set<string> participants;
            for (auto next_index : next_indices)
            {
                auto& reverted_transaction = reverted_transactions_ex[next_index].stx;
                size += 1;
                value += reverted_transaction.transaction_details.fee;

                auto local_participants = action_participants(reverted_transaction);

                participants.insert(local_participants.begin(), local_participants.end());
            }

            used_indices.insert(next_indices.begin(), next_indices.end());
            next_indices.clear();

            for (auto const& participant : participants)
            {
                auto index_it = index_participants.find(participant);
                if (index_it != index_participants.end())
                {
                    for (size_t participant_index : index_it->second)
                    {
                        if (0 == used_indices.count(participant_index))
                            next_indices.insert(participant_index);
                    }
                }
            }
        }

        // share calculated values with all participants
        for (auto const& used_index : used_indices)
        {
            reverted_transactions_ex[used_index].size = size;
            reverted_transactions_ex[used_index].value = value;
        }
    }

    std::sort(reverted_transactions_ex.begin(), reverted_transactions_ex.end(),
              [](extented_info const& lhs, extented_info const& rhs)
    {
        return (lhs.value / lhs.size) > (rhs.value / rhs.size);
    });

    for (size_t index = 0; index != reverted_transactions_ex.size(); ++index)
    {
        auto& signed_transaction = reverted_transactions_ex[index].stx;
        bool can_put_in_block = true;
        
        if (signed_transaction.transaction_details.action.type() == ServiceStatistics::rtt)
        {
            ServiceStatistics* paction;
            signed_transaction.transaction_details.action.get(paction);

            if (paction->start_time_point.tm != system_clock::to_time_t(tp_start) ||
                paction->end_time_point.tm != system_clock::to_time_t(tp_end))
                can_put_in_block = false;
        }
        if (block_transactions.size() < size_t(BLOCK_MAX_TRANSACTIONS) && can_put_in_block)
            block_transactions.push_back(std::move(signed_transaction));
        else
            pool_transactions.push_back(std::move(signed_transaction));
    }

    std::sort(block_transactions.begin(), block_transactions.end(),
              [](SignedTransaction const& lhs, SignedTransaction const& rhs)
    {
        return lhs.transaction_details.creation.tm < rhs.transaction_details.creation.tm;
    });

    // check and copy transactions to block
    for (auto& signed_transaction : block_transactions)
    {
        beltpp::on_failure guard1([] {});
        bool chain_added = impl.m_transaction_cache.add_chain(signed_transaction);
        if (chain_added)
        {
            guard1 = beltpp::on_failure([&impl, &signed_transaction]
            {
                impl.m_transaction_cache.erase_chain(signed_transaction);
            });
        }

        if (chain_added &&
            apply_transaction(signed_transaction, impl, own_key))
        {
            block.signed_transactions.push_back(std::move(signed_transaction));
            guard1.dismiss();
        }
        else
        {
            //  either transaction time corresponds to a future block
            //  or it couldn't be applied because of the order
            //  or it will not even be possible to apply at all
            if (chain_added)
                impl.m_transaction_cache.erase_chain(signed_transaction);
            // because after moving from signed_transaction
            // the guard will be left non functional
            guard1.dismiss();
            pool_transactions.push_back(std::move(signed_transaction));
        }
    }

    std::sort(pool_transactions.begin(), pool_transactions.end(),
              [](SignedTransaction const& lhs, SignedTransaction const& rhs)
    {
        return lhs.transaction_details.creation.tm < rhs.transaction_details.creation.tm;
    });

    //  uri         channel   views
    map<string, map<string, uint64_t>> unit_uri_view_counts;
    //  txid    amount
    map<string, coin> applied_sponsor_items;
    // grant rewards and move to block
    grant_rewards(block.signed_transactions,
                  block.rewards,
                  own_key,
                  block.header,
                  rewards_type::apply,
                  impl,
                  unit_uri_view_counts,
                  applied_sponsor_items);

    meshpp::signature sgn = impl.m_pv_key.sign(block.to_string());

    SignedBlock signed_block;
    signed_block.authorization.address = sgn.pb_key.to_string();
    signed_block.authorization.signature = sgn.base58;
    signed_block.block_details = std::move(block);

    // apply rewards to state
    for (auto& reward : signed_block.block_details.rewards)
        impl.m_state.increase_balance(reward.to, reward.amount, state_layer::chain);

    // insert to blockchain and action_log
    impl.m_blockchain.insert(signed_block);
    impl.m_action_log.log_block(signed_block, unit_uri_view_counts, applied_sponsor_items);

    // apply back rest of the pool content to the state and action_log
    for (auto& signed_transaction : pool_transactions)
    {
        bool complete = action_is_complete(impl, signed_transaction);

        bool ok_logic = true;
        if (complete ||
            false == action_can_apply(impl,
                                      signed_transaction,
                                      signed_transaction.transaction_details.action,
                                      state_layer::pool))
        {
            ok_logic = apply_transaction(signed_transaction, impl);
            if (ok_logic)
                impl.m_action_log.log_transaction(signed_transaction);
        }

        if (ok_logic)
        {
            impl.m_transaction_pool.push_back(signed_transaction);
            impl.m_transaction_cache.add_pool(signed_transaction, complete);
        }
    }

    impl.m_storage_controller.save();
    impl.save(guard);
    impl.m_storage_controller.commit();

    impl.writeln_node("I did it ! " + std::to_string(block_header.block_number) + " block mined :)");
}

void broadcast_node_type(std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    //  don't broadcast if there are no peers
    if (m_pimpl->m_p2p_peers.empty())
        return;

    //  don't broadcast if blockchain is not synced
    if (false == m_pimpl->blockchain_updated())
        return;

    NodeType my_state_type;
    if (m_pimpl->m_state.get_role(m_pimpl->m_pb_key.to_string(), my_state_type))
    {
        assert(my_state_type == m_pimpl->m_node_type);
        return; //  if already stored, do nothing
    }

    if (m_pimpl->m_node_type == BlockchainMessage::NodeType::blockchain)
        return; //  no need to explicitly broadcast in this case

    Role role;
    role.node_address = m_pimpl->m_pb_key.to_string();
    role.node_type = m_pimpl->m_node_type;

    Transaction transaction;
    transaction.action = std::move(role);
    transaction.creation.tm = system_clock::to_time_t(system_clock::now());
    transaction.expiry.tm = system_clock::to_time_t(system_clock::now() + chrono::hours(TRANSACTION_MAX_LIFETIME_HOURS));
    m_pimpl->m_fee_transactions.to_Coin(transaction.fee);

    Authority authorization;
    authorization.address = m_pimpl->m_pb_key.to_string();
    authorization.signature = m_pimpl->m_pv_key.sign(transaction.to_string()).base58;

    SignedTransaction signed_transaction;
    signed_transaction.authorizations.push_back(authorization);
    signed_transaction.transaction_details = transaction;

    if (action_process_on_chain(signed_transaction, *m_pimpl.get()))
    {
        Broadcast broadcast;
        broadcast.package = signed_transaction;

        broadcast_message(std::move(broadcast), *m_pimpl);
    }
}

void broadcast_address_info(std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    NodeType my_state_type;
    if (false == m_pimpl->m_state.get_role(m_pimpl->m_pb_key.to_string(), my_state_type))
        my_state_type = NodeType::blockchain;
    if (m_pimpl->m_public_address.local.empty() &&
        m_pimpl->m_public_address.remote.empty())
        return;

    AddressInfo address_info;
    address_info.node_address = m_pimpl->m_pb_key.to_string();
    beltpp::assign(address_info.ip_address, m_pimpl->m_public_address);
    beltpp::assign(address_info.ssl_ip_address, m_pimpl->m_public_ssl_address);

    Transaction transaction;
    transaction.action = address_info;
    transaction.creation.tm = system_clock::to_time_t(system_clock::now());
    transaction.expiry.tm = system_clock::to_time_t(system_clock::now() + chrono::hours(24));

    Authority authorization;
    authorization.address = m_pimpl->m_pb_key.to_string();
    authorization.signature = m_pimpl->m_pv_key.sign(transaction.to_string()).base58;

    SignedTransaction signed_transaction;
    signed_transaction.transaction_details = transaction;
    signed_transaction.authorizations.push_back(authorization);

    Broadcast broadcast;
    broadcast.package = signed_transaction;

    broadcast_message(std::move(broadcast), *m_pimpl);
}

bool process_address_info(BlockchainMessage::SignedTransaction const& signed_transaction,
                          BlockchainMessage::AddressInfo const& address_info,
                          std::unique_ptr<publiqpp::detail::node_internals>& pimpl)
{
    beltpp::ip_address beltpp_ip_address;
    beltpp::assign(beltpp_ip_address, address_info.ip_address);
    if (beltpp_ip_address.remote.empty() &&
        beltpp_ip_address.local.empty())
        return false;

    beltpp::ip_address beltpp_ssl_ip_address;
    beltpp::assign(beltpp_ssl_ip_address, address_info.ssl_ip_address);

    bool ssl_empty = beltpp_ssl_ip_address.local.empty() && beltpp_ssl_ip_address.remote.empty();
    bool ssl_same = beltpp_ip_address.remote.address == beltpp_ssl_ip_address.remote.address &&
                    beltpp_ip_address.local.address == beltpp_ssl_ip_address.local.address;

    if (false == ssl_same && false == ssl_empty)
        return false;

    // Check data and authority
    if (signed_transaction.authorizations.size() != 1)
        throw wrong_data_exception("transaction authorizations error");

    auto signed_authority = signed_transaction.authorizations.front().address;
    if (signed_authority != address_info.node_address)
        throw authority_exception(signed_authority, address_info.node_address);

    // Check cache
    if (pimpl->m_transaction_cache.contains(signed_transaction))
        return false;

    pimpl->m_transaction_cache.backup();

    beltpp::on_failure guard([&pimpl]
    {
        //pimpl->discard(); // not working with other state
        pimpl->m_transaction_cache.restore();
    });

    //  this is not added to pool, because we don't store it in blockchain
        //  pimpl->m_transaction_pool.push_back(signed_transaction);
    pimpl->m_transaction_cache.add_pool(signed_transaction, true);

    guard.dismiss();

    return true;
}

bool process_update_command(BlockchainMessage::SignedTransaction const& signed_transaction,
                            BlockchainMessage::StorageUpdateCommand const& update_command,
                            std::unique_ptr<publiqpp::detail::node_internals>& pimpl)
{
    // Check data and authority
    NodeType node_type;

    // Check cache
    if (pimpl->m_transaction_cache.contains(signed_transaction))
        return false;

    if (false != update_command.file_uri.empty())
        throw wrong_request_exception("StorageUpdateCommand contains empty file uri!");

    if (update_command.status == UpdateType::store && update_command.channel_address.empty())
        throw wrong_request_exception("StorageUpdateCommand contains wrong data!");

    if (false == pimpl->m_state.get_role(update_command.storage_address, node_type) || node_type != NodeType::storage)
        throw wrong_request_exception("StorageUpdateCommand contains wrong storage address!");

    if (false == update_command.channel_address.empty() &&
        (false == pimpl->m_state.get_role(update_command.channel_address, node_type) || node_type != NodeType::channel))
        throw wrong_request_exception("StorageUpdateCommand contains wrong channel address!");

    if (signed_transaction.authorizations.size() != 1)
        throw wrong_data_exception("transaction authorizations error");

    if (pimpl->m_documents.storage_has_uri(update_command.file_uri, update_command.storage_address))
        return false;

    pimpl->m_transaction_cache.backup();

    beltpp::on_failure guard([&pimpl]
    {
        //pimpl->discard(); // not working with other state
        pimpl->m_transaction_cache.restore();
    });

    //  this is not added to pool, because we don't store it in blockchain
    //  pimpl->m_transaction_pool.push_back(signed_transaction);
    pimpl->m_transaction_cache.add_pool(signed_transaction, true);

    guard.dismiss();

    return true;
}



void broadcast_service_statistics(publiqpp::detail::node_internals& impl)
{
    if (impl.m_node_type == NodeType::blockchain)
        return; // error case should never happen

    ServiceStatistics service_statistics = impl.service_counter.take_statistics_info();

    if (service_statistics.file_items.empty())
        return; // nothing to broadcast

    service_statistics.server_address = impl.m_pb_key.to_string();
    auto tp_end = system_clock::from_time_t(impl.m_blockchain.last_header().time_signed.tm);
    auto tp_start = tp_end - chrono::seconds(BLOCK_MINE_DELAY);
    service_statistics.start_time_point.tm = system_clock::to_time_t(tp_start);
    service_statistics.end_time_point.tm = system_clock::to_time_t(tp_end);

    Transaction transaction;
    transaction.action = std::move(service_statistics);
    transaction.creation.tm = system_clock::to_time_t(system_clock::now());
    transaction.expiry.tm = system_clock::to_time_t(system_clock::now() + chrono::seconds(2 * BLOCK_MINE_DELAY));
    impl.m_fee_transactions.to_Coin(transaction.fee);

    Authority authorization;
    authorization.address = impl.m_pb_key.to_string();
    authorization.signature = impl.m_pv_key.sign(transaction.to_string()).base58;

    SignedTransaction signed_transaction;
    signed_transaction.authorizations.push_back(authorization);
    signed_transaction.transaction_details = transaction;

    if (action_process_on_chain(signed_transaction, impl))
    {
        Broadcast broadcast;
        broadcast.package = signed_transaction;

        broadcast_message(std::move(broadcast), impl);
    }
}

void broadcast_storage_update(publiqpp::detail::node_internals& impl,
                              string const& uri,
                              UpdateType const& status)
{
    StorageUpdate storage_update;
    storage_update.storage_address = impl.m_pb_key.to_string();
    storage_update.file_uri = uri;
    storage_update.status = status;

    Transaction transaction;
    transaction.action = std::move(storage_update);
    transaction.creation.tm = system_clock::to_time_t(system_clock::now());
    transaction.expiry.tm = system_clock::to_time_t(system_clock::now() + chrono::hours(TRANSACTION_MAX_LIFETIME_HOURS));
    impl.m_fee_transactions.to_Coin(transaction.fee);

    Authority authorization;
    authorization.address = impl.m_pb_key.to_string();
    authorization.signature = impl.m_pv_key.sign(transaction.to_string()).base58;

    SignedTransaction signed_transaction;
    signed_transaction.authorizations.push_back(authorization);
    signed_transaction.transaction_details = transaction;

    if (action_process_on_chain(signed_transaction, impl))
    {
        Broadcast broadcast;
        broadcast.package = signed_transaction;

        broadcast_message(std::move(broadcast), impl);
    }
}

void delete_storage_file(publiqpp::detail::node_internals& impl,
                         beltpp::stream* psk,
                         string const& peerid,
                         string const& uri)
{
    auto* pimpl = &impl;
    std::function<void(beltpp::packet&&)> callback_lambda =
        [psk, peerid, uri, pimpl](beltpp::packet&& package)
    {
        if (NodeType::storage == pimpl->m_node_type && package.type() == Done::rtt)
            broadcast_storage_update(*pimpl, uri, UpdateType::remove);

        if (false == package.empty())
            psk->send(peerid, std::move(package));
    };

    vector<unique_ptr<meshpp::session_action<meshpp::session_header>>> actions;
    actions.emplace_back(new session_action_delete_file(impl,
                                                        uri,
                                                        callback_lambda));

    meshpp::session_header header;
    header.peerid = "slave";
    impl.m_sessions.add(header,
                        std::move(actions),
                        chrono::minutes(1));
}

bool process_letter(BlockchainMessage::SignedTransaction const& signed_transaction,
                    BlockchainMessage::Letter const& letter,
                    publiqpp::detail::node_internals& impl)
{
    if (letter.message.size() > 16 * 1024)
        throw too_long_string_exception(letter.message, 16 * 1024);
    if (letter.message.empty())
        throw wrong_data_exception("empty letter.message!");

    meshpp::public_key from(letter.from);
    meshpp::public_key to(letter.to);

    if (signed_transaction.authorizations.size() != 1)
        throw wrong_data_exception("transaction authorizations error");

    if (signed_transaction.authorizations.front().address != letter.from)
        throw authority_exception(signed_transaction.authorizations.front().address, letter.from);

    if (letter.to == letter.from)
        throw wrong_data_exception("sender can read his messages without blockchain!");

    // Check cache
    if (impl.m_transaction_cache.contains(signed_transaction))
        return false;

    impl.m_transaction_cache.backup();
    bool letter_to_me = letter.to == impl.m_pb_key.to_string();

    beltpp::on_failure guard([&impl, letter_to_me]
    {
        if (letter_to_me)
            impl.m_inbox.discard();

        impl.m_transaction_cache.restore();
    });

    if (letter_to_me) // message is addressed to me
    {
        impl.m_inbox.insert(letter);
        impl.m_inbox.save();
    }

    impl.m_transaction_cache.add_pool(signed_transaction, true);

    guard.dismiss();

    if (letter_to_me)
        impl.m_inbox.commit();

    return true;
}

}// end of namespace publiqpp
