#include "communication_p2p.hpp"
#include "communication_rpc.hpp"
#include "transaction_handler.hpp"

#include "coin.hpp"
#include "common.hpp"
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
                         map<string, uint64_t>& map_unit_uri_view_counts,
                         publiqpp::detail::node_internals& impl)
{
    author_result.clear();
    channel_result.clear();
    storage_result.clear();

    map_unit_uri_view_counts.clear();

    map<string, map<string, map<string, uint64_t>>> channel_statistics;
    map<string, map<string, map<string, uint64_t>>> channel_verified_statistics;

    // group channel provided data to verify in comming steps
    for (auto const& stat_item : channel_provided_statistics)
        for (auto const& file_item : stat_item.second.file_items)
            for (auto const& count_item : file_item.count_items)
            {
                string channel_id = stat_item.first;
                string file_uri = file_item.file_uri;
                string storage_id = count_item.peer_address;

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

                uint64_t& stat_value = channel_statistics[channel_id][file_uri][storage_id];

                if (false == stat_mismatch(stat_value, count_item.count))
                    channel_verified_statistics[channel_id][file_uri][storage_id] = stat_value;
            }

    // storage     view
    map<string, uint64_t> storage_group;
    // unit_uri    file_uri   view
    map<string, map<string, uint64_t>> author_group;
    // channel     owner       content_id   view
    map<string, map<string, map<uint64_t, uint64_t>>> content_group;

    for (auto const& stat_item : channel_provided_statistics)
        for (auto const& file_item : stat_item.second.file_items)
            for (auto const& count_item : file_item.count_items)
            {
                string channel_id = stat_item.first;
                string file_uri = file_item.file_uri;
                string unit_uri = file_item.unit_uri;
                string storage_id = count_item.peer_address;
                uint64_t view_count = count_item.count;

                if (channel_verified_statistics[channel_id][file_uri][storage_id] > 0 &&
                    impl.m_documents.exist_unit(unit_uri) && 
                    view_count > 0)
                {
                    storage_group[storage_id] += view_count;
                    author_group[unit_uri][file_uri] += view_count;
                    
                    auto& unit_value = map_unit_uri_view_counts[unit_uri];
                    unit_value = std::max(unit_value, view_count);

                    ContentUnit content_unit = impl.m_documents.get_unit(unit_uri);
                    auto& content_value = content_group[channel_id][content_unit.channel_address][content_unit.content_id];
                    content_value = std::max(content_value, view_count);
                }
            }

    // collect storages final result
    for (auto const& item : storage_group)
        storage_result.insert(make_pair(item.first, make_pair(item.second, 1)));

    // collect channels final result
    for (auto const& item : content_group)
    {
        for (auto const& it : item.second)
        {
            uint64_t count = 0;
            for (auto const& i : it.second)
                count += i.second;

            if (item.first == it.first)
                channel_result.insert(make_pair(item.first, make_pair(count, 1)));
            else
            {
                channel_result.insert(make_pair(it.first, make_pair(count, 2)));
                channel_result.insert(make_pair(item.first, make_pair(count, 2)));
            }
        }
    }

    // collect authors final result
    for (auto const& item : author_group)
    {
        uint64_t total = 0;
        uint64_t file_count = item.second.size();
        for (auto const& it : item.second)
            total += it.second;

        // get middle value as a unit usage
        total = total / file_count;

        for (auto const& it : item.second)
        {
            if (impl.m_documents.exist_file(it.first))
            {
                File file = impl.m_documents.get_file(it.first);
                uint64_t author_count = file.author_addresses.size();

                for (auto const& i : file.author_addresses)
                    author_result.insert(make_pair(i, make_pair(total, file_count * author_count)));
            }
        }
    }
}

coin distribute_rewards(vector<Reward>& rewards,
                        multimap<string, pair<uint64_t, uint64_t>> const& stat_distribution,
                        coin total_amount,
                        RewardType reward_type)
{
    // total_amount will go to miner if no stat distribution
    if (stat_distribution.size() == 0 || total_amount.empty())
        return total_amount;

    map<uint64_t, uint64_t> points_map;
    for (auto const& item : stat_distribution)
        points_map[item.second.second] += item.second.first;

    uint64_t denominator = 1;
    for (auto const& item : points_map)
        denominator *= item.first;

    uint64_t total_points = 0;
    for (auto const& item : points_map)
        total_points += (denominator / item.first) * item.second;

    total_points /= denominator;

    coin rest_amount = total_amount;
    map<string, coin> coin_distribution;
    for (auto const& item : stat_distribution)
    {
        coin amount = (total_amount * item.second.first) / (total_points * item.second.second);

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
                   map<string, uint64_t>& unit_uri_view_counts)
{
    rewards.clear();

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
                if (node_type == NodeType::channel)
                    channel_provided_statistics[service_statistics->server_address] = *service_statistics;
                else
                    storage_provided_statistics[service_statistics->server_address] = *service_statistics;
            }
        }
        else if (it->transaction_details.action.type() == CancelSponsorContentUnit::rtt)
        {
            CancelSponsorContentUnit const* cancel_sponsor_content_unit;
            it->transaction_details.action.get(cancel_sponsor_content_unit);

            auto const& expiry_entry =
                    impl.m_documents.expiration_entry_ref(cancel_sponsor_content_unit->transaction_hash);

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
        map<string, coin> sponsored_rewards =
        impl.m_documents.sponsored_content_unit_set_used(impl,
                                                         unit_uri.first,
                                                         block_header.block_number,
                                                         rewards_type::apply == type ?
                                                             documents::sponsored_content_unit_set_used_apply :
                                                             documents::sponsored_content_unit_set_used_revert,
                                                         string(),  //  transaction_hash_to_validate
                                                         string(),  //  manual_by_account
                                                         false);
        for (auto const& item : sponsored_rewards)
            sponsored_reward += item.second;
    }

    auto expirings = impl.m_documents.content_unit_uri_sponsor_expiring(block_header.block_number);
    for (auto const& expiring_item : expirings)
    {
        auto const& expiring_item_uri = expiring_item.first;
        auto const& expiring_item_transaction_hash = expiring_item.second;
        if (0 == unit_uri_view_counts.count(expiring_item_uri))
        {
            map<string, coin> temp_sponsored_rewards =
                                impl.m_documents.sponsored_content_unit_set_used(impl,
                                                                                 expiring_item_uri,
                                                                                 block_header.block_number,
                                                                                 rewards_type::apply == type ?
                                                                                     documents::sponsored_content_unit_set_used_apply :
                                                                                     documents::sponsored_content_unit_set_used_revert,
                                                                                 expiring_item_transaction_hash,
                                                                                 string(),  //  manual_by_account
                                                                                 false);

            for (auto const& temp_sponsored_reward : temp_sponsored_rewards)
            {
                assert(temp_sponsored_reward.second != coin());
                if (temp_sponsored_reward.second == coin())
                    throw std::logic_error("temp_sponsored_reward.second == coin()");

                auto& sponsored_reward_ref = sponsored_rewards_returns[temp_sponsored_reward.first];
                sponsored_reward_ref += temp_sponsored_reward.second;
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
                   map<string, uint64_t>& unit_uri_view_counts)
{
    vector<Reward> rewards;
    grant_rewards(block.signed_transactions,
                  rewards,
                  authority,
                  block.header,
                  type,
                  impl,
                  unit_uri_view_counts);

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
    auto now = system_clock::now();

    impl.m_transaction_cache.backup();
    beltpp::on_failure guard([&impl]
    {
        impl.discard();
        impl.m_transaction_cache.restore();
    });

    //  collect transactions to be reverted from pool
    //  revert transactions from pool
    vector<SignedTransaction> reverted_transactions =
            revert_pool(system_clock::to_time_t(now), impl);

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

    vector<SignedTransaction> block_transactions;
    vector<SignedTransaction> pool_transactions;

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
        unordered_set<size_t> next_indices = {index};

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
        if (signed_transaction.transaction_details.action.type() ==
            ServiceStatistics::rtt)
        {
            ServiceStatistics* paction;
            signed_transaction.transaction_details.action.get(paction);

            auto tp_end = system_clock::from_time_t(impl.m_blockchain.last_header().time_signed.tm);
            auto tp_start = tp_end - chrono::seconds(BLOCK_MINE_DELAY);

            if (paction->start_time_point.tm != system_clock::to_time_t(tp_start) ||
                paction->end_time_point.tm != system_clock::to_time_t(tp_end))
                can_put_in_block = false;
        }
        if (index < size_t(BLOCK_MAX_TRANSACTIONS) &&
            can_put_in_block)
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
        beltpp::on_failure guard1([]{});
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
            pool_transactions.push_back(std::move(signed_transaction));
        }
    }

    std::sort(pool_transactions.begin(), pool_transactions.end(),
              [](SignedTransaction const& lhs, SignedTransaction const& rhs)
    {
        return lhs.transaction_details.creation.tm < rhs.transaction_details.creation.tm;
    });

    map<string, uint64_t> unit_uri_view_counts;
    // grant rewards and move to block
    grant_rewards(block.signed_transactions,
                  block.rewards,
                  own_key,
                  block.header,
                  rewards_type::apply,
                  impl,
                  unit_uri_view_counts);

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
    impl.m_action_log.log_block(signed_block, unit_uri_view_counts);

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

    impl.save(guard);

    impl.writeln_node("I did it ! " + std::to_string(block_header.block_number) + " block mined :)");
}

void broadcast_node_type(std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    //  don't do anything if there are no peers
    if (m_pimpl->m_p2p_peers.empty())
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

    Authority authorization;
    authorization.address = m_pimpl->m_pb_key.to_string();
    authorization.signature = m_pimpl->m_pv_key.sign(transaction.to_string()).base58;

    SignedTransaction signed_transaction;
    signed_transaction.authorizations.push_back(authorization);
    signed_transaction.transaction_details = transaction;

    if (action_process_on_chain(signed_transaction, *m_pimpl.get()))
    {
        Broadcast broadcast;
        broadcast.echoes = 2;
        broadcast.package = signed_transaction;

        broadcast_message(std::move(broadcast),
                          m_pimpl->m_ptr_p2p_socket->name(),
                          m_pimpl->m_ptr_p2p_socket->name(),
                          true, // broadcast to all peers
                          nullptr, // log disabled
                          m_pimpl->m_p2p_peers,
                          m_pimpl->m_ptr_p2p_socket.get());
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
    broadcast.echoes = 2;
    broadcast.package = signed_transaction;

    broadcast_message(std::move(broadcast),
        m_pimpl->m_ptr_p2p_socket->name(),
        m_pimpl->m_ptr_p2p_socket->name(),
        true, // broadcast to all peers
        nullptr, // log disabled
        m_pimpl->m_p2p_peers,
        m_pimpl->m_ptr_p2p_socket.get());
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

    // Check pool and cache
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
    transaction.expiry.tm = system_clock::to_time_t(system_clock::now() + chrono::seconds(BLOCK_MINE_DELAY));

    Authority authorization;
    authorization.address = impl.m_pb_key.to_string();
    authorization.signature = impl.m_pv_key.sign(transaction.to_string()).base58;

    SignedTransaction signed_transaction;
    signed_transaction.authorizations.push_back(authorization);
    signed_transaction.transaction_details = transaction;

    if (action_process_on_chain(signed_transaction, impl))
    {
        Broadcast broadcast;
        broadcast.echoes = 2;
        broadcast.package = signed_transaction;

        broadcast_message(std::move(broadcast),
            impl.m_ptr_p2p_socket->name(),
            impl.m_ptr_p2p_socket->name(),
            true, // broadcast to all peers
            nullptr, // log disabled
            impl.m_p2p_peers,
            impl.m_ptr_p2p_socket.get());
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

    Authority authorization;
    authorization.address = impl.m_pb_key.to_string();
    authorization.signature = impl.m_pv_key.sign(transaction.to_string()).base58;

    SignedTransaction signed_transaction;
    signed_transaction.authorizations.push_back(authorization);
    signed_transaction.transaction_details = transaction;

    if (action_process_on_chain(signed_transaction, impl))
    {
        Broadcast broadcast;
        broadcast.echoes = 2;
        broadcast.package = signed_transaction;

        broadcast_message(std::move(broadcast),
            impl.m_ptr_p2p_socket->name(),
            impl.m_ptr_p2p_socket->name(),
            true, // broadcast to all peers
            nullptr, // log disabled
            impl.m_p2p_peers,
            impl.m_ptr_p2p_socket.get());
    }
}
}// end of namespace publiqpp
