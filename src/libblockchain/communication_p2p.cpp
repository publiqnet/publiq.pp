#include "communication_p2p.hpp"
#include "communication_rpc.hpp"
#include "transaction_handler.hpp"

#include "coin.hpp"
#include "common.hpp"
#include "exception.hpp"

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
namespace chrono = std::chrono;
using chrono::system_clock;

namespace publiqpp
{
bool apply_transaction(SignedTransaction const& signed_transaction,
                       publiqpp::detail::node_internals& impl,
                       string const& key/* = string()*/)
{
    if (false == action_can_apply(impl, signed_transaction.transaction_details.action))
        return false;

    state_layer layer = state_layer::pool;
    if (false == key.empty())
        layer = state_layer::chain;

    action_apply(impl, signed_transaction.transaction_details.action, layer);

    if (false == fee_can_apply(impl, signed_transaction))
    {
        action_revert(impl, signed_transaction.transaction_details.action, layer);
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

    action_revert(impl, signed_transaction.transaction_details.action, layer);
}

void validate_delations(map<string, ServiceStatistics> const& right_delations,
                        map<string, ServiceStatistics> const& check_delations,
                        map<string, uint64_t>& penals)
{
    penals.clear();
    // to check an algorithm for now

    for (auto const& right_item_it : right_delations)
    {
        for (auto const& right_info_item : right_item_it.second.stat_items)
        {
            bool compare_failed = true;
            auto check_item_it = check_delations.find(right_info_item.peer_address);

            if(check_item_it != check_delations.end())
                for (auto const& check_info_item : check_item_it->second.stat_items)
                {
                    if (check_info_item.peer_address == right_item_it.first)
                    {
                        //  need to check for equality or inequality?
                        if (check_info_item.count == right_info_item.count)
                            compare_failed = false;

                        break;
                    }
                }

            if (compare_failed)
                ++penals[right_info_item.peer_address];
        }
    }
}

void filter_penals(map<string, uint64_t> const& penals, set<string>& result)
{
    result.clear();
    // to check an algorithm for now

    if (penals.empty())
        return;

    uint64_t total_count = penals.size();
    uint64_t remove_count = total_count * 50 / 100; // 50%
    remove_count = remove_count > 0 ? remove_count : 1;

    map<uint64_t, uint64_t> filter_map;

    for (auto const& it : penals)
        ++filter_map[it.second];

    uint64_t threshold = filter_map.cbegin()->first + 1;

    if (filter_map.size() > 1)
    {
        threshold = filter_map.cbegin()->first;

        while (remove_count > filter_map.cbegin()->second)
        {
            filter_map.erase(threshold);
            threshold = filter_map.cbegin()->first;
        }
    }

    for (auto const& it : penals)
        if(it.second < threshold)
            result.insert(it.first);
}

void grant_rewards(vector<SignedTransaction> const& signed_transactions,
                   vector<Reward>& rewards, 
                   string const& address,
                   uint64_t block_number,
                   publiqpp::detail::node_internals& impl)
{
    rewards.clear();

    coin fee = coin(0, 0);
    map<string, uint64_t> channel_penals;
    map<string, uint64_t> storage_penals;
    map<string, ServiceStatistics> channel_delations;
    map<string, ServiceStatistics> storage_delations;

    for (auto it = signed_transactions.begin(); it != signed_transactions.end(); ++it)
    {
        fee += it->transaction_details.fee;

        if (it->transaction_details.action.type() == ServiceStatistics::rtt)
        {
            ServiceStatistics stat_info;
            it->transaction_details.action.get(stat_info);

            NodeType node_type;
            if (impl.m_state.get_role(stat_info.server_address, node_type))
            {
                if (node_type == NodeType::channel)
                {
                    for (auto const& item : stat_info.stat_items)
                    {
                        storage_penals[item.peer_address] = 0;
                        channel_delations[item.peer_address] = stat_info;
                    }
                }
                else if (node_type == NodeType::storage)
                {
                    for (auto const& item : stat_info.stat_items)
                    {
                        channel_penals[item.peer_address] = 0;
                        storage_delations[item.peer_address] = stat_info;
                    }
                }
            }
        }
    }

    size_t year_index = block_number / 50000;
    coin miner_reward, channel_reward, storage_reward;

    if (year_index < 60)
    {
        miner_reward += BLOCK_REWARD_ARRAY[year_index] * MINER_REWARD_PERCENT / 100;
        channel_reward += BLOCK_REWARD_ARRAY[year_index] * CHANNEL_REWARD_PERCENT / 100;
        storage_reward += BLOCK_REWARD_ARRAY[year_index] - miner_reward - channel_reward;
    }

    // grant channel rewards
    set<string> channels;
    validate_delations(channel_delations, storage_delations, storage_penals);
    filter_penals(channel_penals, channels);

    if (channels.size() && !channel_reward.empty())
    {
        Reward reward;
        reward.reward_type = RewardType::channel;
        coin channel_portion = channel_reward / channels.size();

        for (auto const& item : channels)
        {
            reward.to = item;
            channel_portion.to_Coin(reward.amount);

            rewards.push_back(reward);
        }

        (channel_reward - channel_portion * (channels.size() - 1)).to_Coin(rewards.back().amount);
    }
    else
        miner_reward += channel_reward;

    // grant storage rewards
    set<string> storages;
    validate_delations(storage_delations, channel_delations, channel_penals);
    filter_penals(storage_penals, storages);

    if (storages.size() && !storage_reward.empty())
    {
        Reward reward;
        reward.reward_type = RewardType::storage;
        coin storage_portion = storage_reward / storages.size();

        for (auto const& item : storages)
        {
            reward.to = item;
            storage_portion.to_Coin(reward.amount);

            rewards.push_back(reward);
        }

        (storage_reward - storage_portion * (storages.size() - 1)).to_Coin(rewards.back().amount);
    }
    else
        miner_reward += storage_reward;

    // grant miner reward himself
    if (!miner_reward.empty())
    {
        Reward reward;
        reward.to = address;
        miner_reward.to_Coin(reward.amount);
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

bool check_rewards(Block const& block, string const& authority,
                   publiqpp::detail::node_internals& impl)
{
    vector<Reward> rewards;
    grant_rewards(block.signed_transactions, rewards, authority, block.header.block_number, impl);

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

void broadcast_storage_info(publiqpp::detail::node_internals& impl)
{//TODO
    vector<string> storages = impl.m_state.get_nodes_by_type(NodeType::storage);

    if (storages.empty()) return;

    //uint64_t block_number = impl.m_blockchain.length() - 1;
    //SignedBlock const& signed_block = impl.m_blockchain.at(block_number);

    ServiceStatistics stat_info;
    //  use block number instead, or something similar
    //stat_info.hash = meshpp::hash(signed_block.block_details.to_string());
    stat_info.server_address = impl.m_pb_key.to_string();

    for (auto& nodeid : storages)
    {
        ServiceStatisticsItem stat_item;
        stat_item.peer_address = nodeid;
        //stat_item.content_hash = meshpp::hash("storage");
        stat_item.count = 1;
        //stat_item.failed = 0;

        stat_info.stat_items.push_back(stat_item);
    }

    Transaction transaction;
    transaction.action = stat_info;
    transaction.creation.tm = system_clock::to_time_t(system_clock::now());
    transaction.expiry.tm = system_clock::to_time_t(system_clock::now() + chrono::minutes(10));

    Authority authorization;
    authorization.address = impl.m_pb_key.to_string();
    authorization.signature = impl.m_pv_key.sign(transaction.to_string()).base58;

    SignedTransaction signed_transaction;
    signed_transaction.authorizations.push_back(authorization);
    signed_transaction.transaction_details = transaction;

    Broadcast broadcast;
    broadcast.echoes = 2;
    broadcast.package = signed_transaction;

    broadcast_message(std::move(broadcast),
                      impl.m_ptr_p2p_socket->name(),
                      impl.m_ptr_p2p_socket->name(),
                      true,
                      nullptr,
                      impl.m_p2p_peers,
                      impl.m_ptr_p2p_socket.get());
}

uint64_t check_delta_vector(vector<pair<uint64_t, uint64_t>> const& delta_vector, std::string& error)
{
    static_assert(DELTA_STEP > 0, "check this please");
    assert(false == delta_vector.empty());
    if (delta_vector.empty())
        throw std::logic_error("check_delta_vector(): delta_vector.empty()");

    uint64_t expected_c_const = delta_vector.front().second;
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

        size_t delta_sum = 0;
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

void revert_pool(time_t expiry_time, 
                 publiqpp::detail::node_internals& impl,
                 multimap<BlockchainMessage::ctime, SignedTransaction>& pool_transactions)
{
    //  collect transactions to be reverted from pool
    //
    pool_transactions.clear();
    size_t state_pool_size = impl.m_transaction_pool.length();

    for (size_t index = 0; index != state_pool_size; ++index)
    {
        SignedTransaction const& signed_transaction = impl.m_transaction_pool.at(index);

        if (expiry_time <= signed_transaction.transaction_details.expiry.tm)
        {
            pool_transactions.insert(
                        std::make_pair(signed_transaction.transaction_details.creation, signed_transaction));
        }
    }

    //  revert transactions from pool
    //
    for (size_t index = state_pool_size; index != 0; --index)
    {
        SignedTransaction const& signed_transaction = impl.m_transaction_pool.at(index - 1);

        impl.m_transaction_pool.pop_back();
        bool complete = impl.m_transaction_cache.erase_pool(signed_transaction);

        if (complete)
        {
            impl.m_action_log.revert();
            revert_transaction(signed_transaction, impl);
        }
    }

    assert(impl.m_transaction_pool.length() == 0);
}

void mine_block(unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    auto now = system_clock::now();

    m_pimpl->m_transaction_cache.backup();
    beltpp::on_failure guard([&m_pimpl]
    {
        m_pimpl->discard();
        m_pimpl->m_transaction_cache.restore();
    });

    multimap<BlockchainMessage::ctime, SignedTransaction> pool_transactions;
    //  collect transactions to be reverted from pool
    //  revert transactions from pool
    revert_pool(system_clock::to_time_t(now), *m_pimpl.get(), pool_transactions);

    uint64_t block_number = m_pimpl->m_blockchain.length() - 1;

    //  calculate consensus_const
    vector<pair<uint64_t, uint64_t>> delta_vector;
    size_t index = 0;
    if (block_number + 1 >= DELTA_STEP)
        index = block_number + 1 - DELTA_STEP;
    for (; index <= block_number; ++index)
    {
        BlockHeader const& tmp_header = m_pimpl->m_blockchain.header_at(index);
        delta_vector.push_back(std::make_pair(tmp_header.delta, tmp_header.c_const));
    }

    assert(false == delta_vector.empty());

    string check_delta_vector_error;
    uint64_t calculate_c_const = check_delta_vector(delta_vector, check_delta_vector_error);
    assert(check_delta_vector_error.empty());
    if (false == check_delta_vector_error.empty())
        throw std::logic_error("own blockchain is somehow wrong");

    SignedBlock const& prev_signed_block = m_pimpl->m_blockchain.at(block_number);
    BlockHeader const& prev_header = m_pimpl->m_blockchain.last_header();
    string own_key = m_pimpl->m_pb_key.to_string();
    string prev_hash = meshpp::hash(prev_signed_block.block_details.to_string());

    uint64_t delta = m_pimpl->calc_delta(own_key,
                                         m_pimpl->get_balance().whole,
                                         prev_hash,
                                         calculate_c_const);

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

    // check and copy transactions to block
    size_t transactions_count = 0;
    auto it = pool_transactions.begin();
    while (it != pool_transactions.end() && transactions_count < size_t(BLOCK_MAX_TRANSACTIONS))
    {
        auto& signed_transaction = it->second;
        auto code = action_authorization_process(*m_pimpl.get(), signed_transaction);

        if (code.complete &&
            signed_transaction.transaction_details.creation < block_header.time_signed &&
            apply_transaction(signed_transaction, *m_pimpl.get(), own_key))
        {
            m_pimpl->m_transaction_cache.add_chain(signed_transaction);
            block.signed_transactions.push_back(std::move(signed_transaction));

            ++transactions_count;
            it = pool_transactions.erase(it);
        }
        else
        {
            //  either transaction time corresponds to a future block
            //  or it couldn't be applied because of the order
            //  or it will not even be possible to apply at all
            ++it;
        }
    }

    // grant rewards and move to block
    grant_rewards(block.signed_transactions, block.rewards, own_key, block.header.block_number, *m_pimpl.get());

    meshpp::signature sgn = m_pimpl->m_pv_key.sign(block.to_string());

    SignedBlock signed_block;
    signed_block.authorization.address = sgn.pb_key.to_string();
    signed_block.authorization.signature = sgn.base58;
    signed_block.block_details = block;

    // apply rewards to state and action_log
    for (auto& reward : block.rewards)
        m_pimpl->m_state.increase_balance(reward.to, reward.amount, state_layer::chain);

    // insert to blockchain and action_log
    m_pimpl->m_blockchain.insert(signed_block);
    m_pimpl->m_action_log.log_block(signed_block);

    // apply back rest of the pool content to the state and action_log
    for (auto& item : pool_transactions)
    {
        auto& signed_transaction = item.second;
        auto code = action_authorization_process(*m_pimpl.get(), signed_transaction);
        bool complete = code.complete;

        bool ok_logic = true;
        if (complete ||
            false == action_can_apply(*m_pimpl.get(), signed_transaction.transaction_details.action))
        {
            ok_logic = apply_transaction(signed_transaction, *m_pimpl.get());
            if (ok_logic)
                m_pimpl->m_action_log.log_transaction(signed_transaction);
        }

        if (ok_logic)
        {
            m_pimpl->m_transaction_pool.push_back(signed_transaction);
            m_pimpl->m_transaction_cache.add_pool(signed_transaction, complete);
        }
    }

    m_pimpl->save(guard);

    m_pimpl->writeln_node("new block mined : " + std::to_string(block_header.block_number));
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

    if (broadcast_type::none != action_process_on_chain(signed_transaction, *m_pimpl.get()))
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

void broadcast_storage_stat(ServiceStatistics& service_statistics,
                            std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    unordered_set<string> channels_set;
    vector<string> channels = m_pimpl->m_state.get_nodes_by_type(NodeType::channel);

    if (channels.empty()) return;

    for (auto& channel_node_address : channels)
        channels_set.insert(channel_node_address);

    auto it = service_statistics.stat_items.begin();
    while (it != service_statistics.stat_items.end())
    {
        if (channels_set.count(it->peer_address))
            ++it;
        else
            it = service_statistics.stat_items.erase(it);
    }

    if (service_statistics.stat_items.empty()) return;

    //uint64_t block_number = m_pimpl->m_blockchain.length() - 1;
    //SignedBlock const& signed_block = m_pimpl->m_blockchain.at(block_number);

    //  use something else instead of hash, block number maybe
    //service_statistics.hash = meshpp::hash(signed_block.block_details.to_string());
    service_statistics.server_address = m_pimpl->m_pb_key.to_string();

    Transaction transaction;
    transaction.action = service_statistics;
    transaction.creation.tm = system_clock::to_time_t(system_clock::now());
    transaction.expiry.tm = system_clock::to_time_t(system_clock::now() + chrono::minutes(10));

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
        true,
        nullptr,
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
}// end of namespace publiqpp
