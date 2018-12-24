#include "communication_p2p.hpp"
#include "communication_rpc.hpp"

#include "coin.hpp"
#include "common.hpp"

#include <mesh.pp/cryptoutility.hpp>

#include <map>
#include <set>

using namespace BlockchainMessage;

using std::map;
using std::set;

///////////////////////////////////////////////////////////////////////////////////
//                            Internal Finctions
bool apply_transaction(SignedTransaction const& signed_transaction,
                       unique_ptr<publiqpp::detail::node_internals>& m_pimpl, 
                       string const& key = string())
{
    coin fee;

    if (!key.empty())
        fee = signed_transaction.transaction_details.fee;

    if (signed_transaction.transaction_details.action.type() == Transfer::rtt)
    {
        Transfer transfer;
        signed_transaction.transaction_details.action.get(transfer);

        if (!key.empty())
        {
            Coin balance = m_pimpl->m_state.get_balance(transfer.from);

            if (coin(balance) < transfer.amount + fee)
                return false;
        }

        m_pimpl->m_state.decrease_balance(transfer.from, transfer.amount + fee);
        m_pimpl->m_state.increase_balance(transfer.to, transfer.amount);
    }
    else if (signed_transaction.transaction_details.action.type() == Contract::rtt)
    {
        Contract contract;
        signed_transaction.transaction_details.action.get(contract);

        if (m_pimpl->m_state.get_contract_type(contract.owner) != NodeType::miner)
            return false; // technical solution to remove used contract

        if (!key.empty())
        {
            Coin balance = m_pimpl->m_state.get_balance(signed_transaction.authority);

            if (coin(balance) < fee)
                return false;

            m_pimpl->m_state.insert_contract(contract);
            m_pimpl->m_state.decrease_balance(signed_transaction.authority, fee);
        }
    }
    else if (signed_transaction.transaction_details.action.type() == ArticleInfo::rtt)
    {
        if (!key.empty())
        {
            Coin balance = m_pimpl->m_state.get_balance(signed_transaction.authority);

            if (coin(balance) < fee)
                return false;

            m_pimpl->m_state.decrease_balance(signed_transaction.authority, fee);
        }
    }
    else if (signed_transaction.transaction_details.action.type() == StatInfo::rtt)
    {
        if (!key.empty())
        {
            StatInfo stat_info;
            signed_transaction.transaction_details.action.get(stat_info);

            if (stat_info.hash != m_pimpl->m_blockchain.last_hash())
                return false;

            Coin balance = m_pimpl->m_state.get_balance(signed_transaction.authority);

            if (coin(balance) < fee)
                return false;

            m_pimpl->m_state.decrease_balance(signed_transaction.authority, fee);
        }
    }
    else
        throw wrong_data_exception("unknown transaction action type!");

    if (!fee.empty())
        m_pimpl->m_state.increase_balance(key, fee);

    return true;
}

void revert_transaction(SignedTransaction const& signed_transaction,
                        unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                        bool const increase,
                        string const& key = string())
{
    coin fee;

    if (!key.empty())
    {
        fee = signed_transaction.transaction_details.fee;

        if (!increase)
            m_pimpl->m_state.decrease_balance(key, fee);
    }

    if (signed_transaction.transaction_details.action.type() == Transfer::rtt)
    {
        Transfer transfer;
        signed_transaction.transaction_details.action.get(transfer);

        if (increase)
            m_pimpl->m_state.increase_balance(transfer.from, transfer.amount + fee);
        else
            m_pimpl->m_state.decrease_balance(transfer.to, transfer.amount);
    }
    else if (signed_transaction.transaction_details.action.type() == Contract::rtt)
    {
        if (increase)
            m_pimpl->m_state.increase_balance(signed_transaction.authority, fee);
        else
        {
            Contract contract;
            signed_transaction.transaction_details.action.get(contract);

            m_pimpl->m_state.remove_contract(contract);
        }
    }
    else if (signed_transaction.transaction_details.action.type() == ArticleInfo::rtt)
    {
        if (!fee.empty())
        {
            if (increase)
                m_pimpl->m_state.increase_balance(signed_transaction.authority, fee);
        }
    }
    else if (signed_transaction.transaction_details.action.type() == StatInfo::rtt)
    {
        if (!fee.empty())
        {
            if (increase)
                m_pimpl->m_state.increase_balance(signed_transaction.authority, fee);
        }
    }
    else
        throw wrong_data_exception("unknown transaction action type!");
}

void validate_delations(map<string, StatInfo> const& right_delations,
                        map<string, StatInfo> const& check_delations,
                        map<string, uint64_t>& penals)
{
    penals.clear();
    // to check an algorithm for now

    for (auto const& right_item_it : right_delations)
    {
        for (auto const& right_info_item : right_item_it.second.items)
        {
            bool compare_failed = true;
            auto check_item_it = check_delations.find(right_info_item.node);

            if(check_item_it != check_delations.end())
                for (auto const& check_info_item : check_item_it->second.items)
                {
                    if (check_info_item.node == right_item_it.first)
                    {
                        if (check_info_item.passed == right_info_item.passed)
                            compare_failed = false;

                        break;
                    }
                }

            if (compare_failed)
                ++penals[right_info_item.node];
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
                   string const& authority, 
                   uint64_t block_number,
                   std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    rewards.clear();

    map<string, uint64_t> channel_penals;
    map<string, uint64_t> storage_penals;
    map<string, StatInfo> channel_delations;
    map<string, StatInfo> storage_delations;

    for (auto it = signed_transactions.begin(); it != signed_transactions.end(); ++it)
    {
        if (it->transaction_details.action.type() == StatInfo::rtt)
        {
            StatInfo stat_info;
            it->transaction_details.action.get(stat_info);

            NodeType authority_type = m_pimpl->m_state.get_contract_type(it->authority);

            if (authority_type == NodeType::channel)
            {
                for (auto const& item : stat_info.items)
                {
                    storage_penals[item.node] = 0;
                    channel_delations[item.node] = stat_info;
                }
            }
            else if (authority_type == NodeType::storage)
            {
                for (auto const& item : stat_info.items)
                {
                    channel_penals[item.node] = 0;
                    storage_delations[item.node] = stat_info;
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
        coin channel_portion = channel_reward / channels.size();

        for (auto const& item : channels)
        {
            reward.to = item;
            reward.amount = channel_portion.to_Coin();

            rewards.push_back(reward);
        }

        rewards.back().amount = (channel_reward - channel_portion * (channels.size() - 1)).to_Coin();
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
        coin storage_portion = storage_reward / storages.size();

        for (auto const& item : storages)
        {
            reward.to = item;
            reward.amount = storage_portion.to_Coin();

            rewards.push_back(reward);
        }

        rewards.back().amount = (storage_reward - storage_portion * (storages.size() - 1)).to_Coin();
    }
    else
        miner_reward += storage_reward;

    // grant miner reward himself
    if (!miner_reward.empty())
    {
        Reward reward;
        reward.to = authority;
        reward.amount = miner_reward.to_Coin();

        rewards.push_back(reward);
    }
}

bool check_headers(BlockHeader const& next_header, BlockHeader const& header)
{
    bool t = next_header.block_number != header.block_number + 1;
    t = t || next_header.c_sum <= header.c_sum;
    t = t || next_header.c_sum != next_header.delta + header.c_sum;
    t = t || (next_header.c_const != header.c_const &&
              next_header.c_const != header.c_const * 2 && 
              next_header.c_const != header.c_const / 2);

    system_clock::time_point time_point1 = system_clock::from_time_t(header.time_signed.tm);
    system_clock::time_point time_point2 = system_clock::from_time_t(next_header.time_signed.tm);
    chrono::seconds diff_seconds = chrono::duration_cast<chrono::seconds>(time_point2 - time_point1);

    return t || time_point1 > time_point2 || diff_seconds.count() < BLOCK_MINE_DELAY;
};

bool check_rewards(Block const& block, string const& authority,
                   std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    vector<Reward> rewards;
    grant_rewards(block.signed_transactions, rewards, authority, block.header.block_number, m_pimpl);

    auto it1 = rewards.begin();
    auto it2 = block.rewards.begin();

    bool bad_reward = rewards.size() != block.rewards.size();

    while ( !bad_reward && it1 != rewards.end())
    {
        bad_reward = *it1 != *it2;

        ++it1;
        ++it2;
    }

    return bad_reward;
}

void broadcast_storage_info(std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{//TODO
    vector<Contract> storages;
    m_pimpl->m_state.get_contracts(storages, NodeType::storage);

    if (storages.empty()) return;

    SignedBlock signed_block;
    uint64_t block_number = m_pimpl->m_blockchain.length() - 1;
    m_pimpl->m_blockchain.at(block_number, signed_block);

    StatInfo storage_info;
    storage_info.hash = meshpp::hash(signed_block.block_details.to_string());

    for (auto& contract : storages)
    {
        StatItem stat_item;
        stat_item.node = contract.owner;
        //stat_item.content_hash = meshpp::hash("storage");
        stat_item.passed = 1;
        stat_item.failed = 0;

        storage_info.items.push_back(stat_item);
    }

    Transaction transaction;
    transaction.action = storage_info;
    transaction.creation.tm = system_clock::to_time_t(system_clock::now());
    transaction.expiry.tm = system_clock::to_time_t(system_clock::now() + chrono::minutes(10));

    SignedTransaction signed_transaction;
    signed_transaction.authority = m_pimpl->m_pb_key.to_string();
    signed_transaction.transaction_details = transaction;
    signed_transaction.signature = m_pimpl->m_pv_key.sign(transaction.to_string()).base58;

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

void broadcast_channel_info(std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    StatInfo channel_info;

    if (m_pimpl->m_stat_counter.get_stat_info(channel_info))
    {
        Transaction transaction;
        transaction.action = channel_info;
        transaction.creation.tm = system_clock::to_time_t(system_clock::now());
        transaction.expiry.tm = system_clock::to_time_t(system_clock::now() + chrono::minutes(10));

        SignedTransaction signed_transaction;
        signed_transaction.authority = m_pimpl->m_pb_key.to_string();
        signed_transaction.transaction_details = transaction;
        signed_transaction.signature = m_pimpl->m_pv_key.sign(transaction.to_string()).base58;

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

    SignedBlock signed_block;
    m_pimpl->m_blockchain.at(m_pimpl->m_blockchain.length() - 1, signed_block);
    m_pimpl->m_stat_counter.init(meshpp::hash(signed_block.block_details.to_string()));
}

///////////////////////////////////////////////////////////////////////////////////

void mine_block(unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    SignedBlock prev_signed_block;
    uint64_t block_number = m_pimpl->m_blockchain.length() - 1;
    m_pimpl->m_blockchain.at(block_number, prev_signed_block);

    BlockHeader prev_header;
    m_pimpl->m_blockchain.last_header(prev_header);

    string own_key = m_pimpl->m_pb_key.to_string();
    string prev_hash = meshpp::hash(prev_signed_block.block_details.to_string());
    uint64_t delta = m_pimpl->calc_delta(own_key, m_pimpl->m_balance.whole, prev_hash, prev_header.c_const);

    // fill new block header data
    BlockHeader block_header;
    block_header.block_number = block_number + 1;
    block_header.delta = delta;
    block_header.c_const = prev_header.c_const;
    block_header.c_sum = prev_header.c_sum + delta;
    block_header.prev_hash = prev_hash;
    block_header.time_signed.tm = system_clock::to_time_t(system_clock::now());

    // update consensus_const if needed
    if (delta > DELTA_UP)
    {
        size_t step = 0;
        BlockHeader prev_header_local;
        m_pimpl->m_blockchain.header_at(block_number, prev_header_local);

        while (prev_header_local.delta > DELTA_UP &&
            step <= DELTA_STEP && prev_header_local.block_number > 0)
        {
            ++step;
            m_pimpl->m_blockchain.header_at(prev_header_local.block_number - 1, prev_header_local);
        }

        // -1 because current delta is not counted
        if (step >= DELTA_STEP - 1)
            block_header.c_const = prev_header.c_const * 2;
    }
    else
        if (delta < DELTA_DOWN && block_header.c_const > 1)
        {
            size_t step = 0;
            BlockHeader prev_header_local;
            m_pimpl->m_blockchain.header_at(block_number, prev_header_local);

            while (prev_header_local.delta < DELTA_DOWN &&
                step <= DELTA_STEP && prev_header_local.block_number > 0)
            {
                ++step;
                m_pimpl->m_blockchain.header_at(prev_header_local.block_number - 1, prev_header_local);
            }

            // -1 because current delta is not counted
            if (step >= DELTA_STEP - 1)
                block_header.c_const = prev_header.c_const / 2;
        }

    Block block;
    block.header = block_header;

    beltpp::on_failure guard([&m_pimpl] 
    { 
        m_pimpl->discard();
        m_pimpl->calc_balance();
    });

    std::multimap<BlockchainMessage::ctime, std::pair<string, SignedTransaction>> transaction_map;

    // Revert action log and state, revert transaction pool records
    vector<string> pool_keys;
    vector<SignedTransaction> pool_transactions;
    m_pimpl->m_transaction_pool.get_keys(pool_keys);
    for (auto& key : pool_keys)
    {
        SignedTransaction signed_transaction;
        m_pimpl->m_transaction_pool.at(key, signed_transaction);

        pool_transactions.push_back(signed_transaction);

        m_pimpl->m_action_log.revert();
        revert_transaction(signed_transaction, m_pimpl, true);

        if (block_header.time_signed.tm > signed_transaction.transaction_details.expiry.tm)
            m_pimpl->m_transaction_pool.remove(key); // already expired transaction
        else
            transaction_map.insert(std::pair<BlockchainMessage::ctime, std::pair<string, SignedTransaction>>(
                                   signed_transaction.transaction_details.creation,
                                   std::pair<string, SignedTransaction>(key, signed_transaction)));
    }

    // complete revert started above
    for (auto& signed_transaction : pool_transactions)
        revert_transaction(signed_transaction, m_pimpl, false);

    // check and copy transactions to block
    size_t tr_count = 0;
    auto it = transaction_map.begin();
    for (; it != transaction_map.end() && tr_count < BLOCK_MAX_TRANSACTIONS; ++it)
    {
        // Check block transactions and calculate new state
        if (apply_transaction(it->second.second, m_pimpl, own_key))
        {
            if(it->second.second.transaction_details.action.type() != StatInfo::rtt)
                ++tr_count;

            block.signed_transactions.push_back(std::move(it->second.second));
            m_pimpl->m_transaction_cache[it->second.first] = system_clock::from_time_t(it->first.tm);
        }

        m_pimpl->m_transaction_pool.remove(it->second.first);
    }

    // grant rewards and move to block
    grant_rewards(block.signed_transactions, block.rewards, own_key, block.header.block_number, m_pimpl);

    meshpp::signature sgn = m_pimpl->m_pv_key.sign(block.to_string());

    SignedBlock signed_block;
    signed_block.signature = sgn.base58;
    signed_block.authority = sgn.pb_key.to_string();
    signed_block.block_details = block;

    // apply rewards to state and action_log
    for (auto& reward : block.rewards)
        m_pimpl->m_state.increase_balance(reward.to, reward.amount);

    // insert to blockchain and action_log
    m_pimpl->m_blockchain.insert(signed_block);
    m_pimpl->m_action_log.log_block(signed_block);

    // calculate miner balance
    m_pimpl->calc_balance();

    // apply back rest of the pool content to the state and action_log
    for (; it != transaction_map.end(); ++it)
        if (apply_transaction(it->second.second, m_pimpl))
            m_pimpl->m_action_log.log_transaction(it->second.second);
        else
            m_pimpl->m_transaction_pool.remove(it->second.first);

    m_pimpl->save(guard);
    m_pimpl->calc_sync_info(block);

    m_pimpl->writeln_node("new block mined : " + std::to_string(block_header.block_number));
}

void process_blockheader_request(BlockHeaderRequest const& header_request,
                                 std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                                 beltpp::isocket& sk,
                                 beltpp::isocket::peer_id const& peerid)
{
    // headers always requested in reverse order!

    uint64_t from = m_pimpl->m_blockchain.length() - 1;
    from = from < header_request.blocks_from ? from : header_request.blocks_from;

    uint64_t to = header_request.blocks_to;
    to = to > from ? from : to;
    to = from > HEADER_TR_LENGTH && to < from - HEADER_TR_LENGTH ? from - HEADER_TR_LENGTH : to;

    BlockHeaderResponse header_response;
    for (auto index = from + 1; index > to; --index)
    {
        BlockHeader header;
        m_pimpl->m_blockchain.header_at(index - 1, header);

        header_response.block_headers.push_back(std::move(header));
    }

    sk.send(peerid, header_response);
}

void process_blockheader_response(BlockHeaderResponse&& header_response,
                                  std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                                  beltpp::isocket& sk,
                                  beltpp::isocket::peer_id const& peerid)
{
    // find needed header from own data
    BlockHeader tmp_header;
    m_pimpl->m_blockchain.last_header(tmp_header);

    // validate received headers
    if (header_response.block_headers.empty())
        throw wrong_data_exception("blockheader response. empty response received!");

    auto r_it = header_response.block_headers.begin();
    if (r_it->block_number == tmp_header.block_number && m_pimpl->sync_headers.empty() &&
        r_it->c_sum <= tmp_header.c_sum)
        throw wrong_data_exception("blockheader response. wrong data received!");

    if (!m_pimpl->sync_headers.empty() && // we have something received before
        tmp_header.block_number >= m_pimpl->sync_headers.rbegin()->block_number)
    {
        // load next mot checked header
        m_pimpl->m_blockchain.header_at(m_pimpl->sync_headers.rbegin()->block_number - 1, tmp_header);

        if (tmp_header.block_number != r_it->block_number)
            throw wrong_data_exception("blockheader response. unexpected data received!");
    }

    //-----------------------------------------------------//
    auto check_headers_vector = [](vector<BlockHeader> const& header_vector)
    {
        bool t = false;
        auto it = header_vector.begin();
        for (++it; !t && it != header_vector.end(); ++it)
            t = check_headers(*(it - 1), *it);

        return t;
    };
    //-----------------------------------------------------//

    if (check_headers_vector(header_response.block_headers))
        throw wrong_data_exception("blockheader response. wrong data in response!");

    SignedBlock tmp_block;
    m_pimpl->m_blockchain.at(tmp_header.block_number, tmp_block);
    
    string tmp_hash = meshpp::hash(tmp_block.block_details.to_string());

    // find last common header
    bool found = false;
    bool lcb_found = false;
    r_it = header_response.block_headers.begin();
    while (!found && r_it != header_response.block_headers.end())
    {
        if (r_it->prev_hash == tmp_hash &&
            r_it->block_number == tmp_header.block_number + 1)
        {
            found = true;
            lcb_found = true;
            m_pimpl->sync_headers.push_back(std::move(*r_it));
        }
        else if (r_it->block_number > tmp_header.block_number)
            m_pimpl->sync_headers.push_back(std::move(*r_it++));
        else
            found = true;
    }

    if (found)
    {
        while (!lcb_found &&
               r_it != header_response.block_headers.end() &&
               r_it->c_sum > tmp_header.c_sum)
        {
            m_pimpl->sync_headers.push_back(std::move(*r_it++));
            m_pimpl->m_blockchain.header_at(tmp_header.block_number - 1, tmp_header);
        }

        for (; !lcb_found && r_it != header_response.block_headers.end(); ++r_it)
        {
            if (tmp_header.prev_hash == r_it->prev_hash)
                lcb_found = true;

            if (tmp_header.block_number > 0)
            {
                m_pimpl->sync_headers.push_back(std::move(*r_it));
                m_pimpl->m_blockchain.header_at(tmp_header.block_number - 1, tmp_header);
            }
        }

        if (lcb_found)
        {
            if (check_headers(*m_pimpl->sync_headers.rbegin(), tmp_header) ||
                check_headers_vector(m_pimpl->sync_headers))
                throw wrong_data_exception("blockheader response. header check failed!");

            // verify consensus_const
            vector<pair<uint64_t, uint64_t>> delta_vector;

            for (auto const& item : m_pimpl->sync_headers)
                delta_vector.push_back(pair<uint64_t, uint64_t>(item.delta, item.c_const));
        
            uint64_t number = m_pimpl->sync_headers.rbegin()->block_number - 1;
            uint64_t delta_step = number < DELTA_STEP ? number : DELTA_STEP;
        
            for (uint64_t i = 0; i < delta_step; ++i)
            {
                BlockHeader _header;
                m_pimpl->m_blockchain.header_at(number - i, _header);
        
                delta_vector.push_back(pair<uint64_t, uint64_t>(_header.delta, _header.c_const));
            }
        
            for (auto it = delta_vector.begin(); it + delta_step != delta_vector.end(); ++it)
            {
                if (it->first > DELTA_UP)
                {
                    size_t step = 0;
                    uint64_t _delta = it->first;
        
                    while (_delta > DELTA_UP && step < DELTA_STEP && it + step != delta_vector.end())
                    {
                        ++step;
                        _delta = (it + step)->first;
                    }
        
                    if (step >= DELTA_STEP && it->second != (it + 1)->second * 2)
                        throw wrong_data_exception("blockheader response. wrong consensus const up !");
                }
                else if (it->first < DELTA_DOWN && it->second > 1)
                {
                    size_t step = 0;
                    uint64_t _delta = it->first;
        
                    while (_delta < DELTA_DOWN && step < DELTA_STEP && it + step != delta_vector.end())
                    {
                        ++step;
                        _delta = (it + step)->first;
                    }
        
                    if (step >= DELTA_STEP && it->second != (it + 1)->second / 2)
                        throw wrong_data_exception("blockheader response. wrong consensus const down !");
                }
            }

            //3. request blockchain from found point
            BlockchainRequest blockchain_request;
            blockchain_request.blocks_from = m_pimpl->sync_headers.rbegin()->block_number;
            blockchain_request.blocks_to = m_pimpl->sync_headers.begin()->block_number;

            sk.send(peerid, blockchain_request);
            m_pimpl->update_sync_time();
            m_pimpl->store_request(peerid, blockchain_request);

            return;
        }
    }

    if (!found || !lcb_found)
    {
        // request more headers
        BlockHeaderRequest header_request;
        header_request.blocks_from = m_pimpl->sync_headers.rbegin()->block_number - 1;
        header_request.blocks_to = header_request.blocks_from > HEADER_TR_LENGTH ? header_request.blocks_from - HEADER_TR_LENGTH : 0;

        sk.send(peerid, header_request);
        m_pimpl->update_sync_time();
        m_pimpl->store_request(peerid, header_request);
    }
}

void process_blockchain_request(BlockchainRequest const& blockchain_request,
                                std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                                beltpp::isocket& sk,
                                beltpp::isocket::peer_id const& peerid)
{
    // blocks are always requested in regular order

    uint64_t number = m_pimpl->m_blockchain.length() - 1;
    uint64_t from = number < blockchain_request.blocks_from ? number : blockchain_request.blocks_from;

    uint64_t to = blockchain_request.blocks_to;
    to = to < from ? from : to;
    to = to > from + BLOCK_TR_LENGTH ? from + BLOCK_TR_LENGTH : to;
    to = to > number ? number : to;

    BlockchainResponse chain_response;
    for (auto i = from; i <= to; ++i)
    {
        SignedBlock signed_block;
        m_pimpl->m_blockchain.at(i, signed_block);

        chain_response.signed_blocks.push_back(std::move(signed_block));
    }

    sk.send(peerid, chain_response);
}

void process_blockchain_response(BlockchainResponse&& response,
                                 std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                                 beltpp::isocket& sk,
                                 beltpp::isocket::peer_id const& peerid)
{
    //1. check received blockchain validity

    if (response.signed_blocks.empty())
        throw wrong_data_exception("blockchain response. empty response received!");

    // find last common block
    uint64_t block_number = m_pimpl->sync_headers.back().block_number;

    if (block_number == 0) throw wrong_data_exception("blockchain response. uzum en qcen!");

    //2. check and add received blockchain to sync_blocks_vector for future process
    size_t length = m_pimpl->sync_blocks.size();

    // put prev_signed_block in correct place
    SignedBlock prev_signed_block;
    if (m_pimpl->sync_blocks.empty())
        m_pimpl->m_blockchain.at(block_number - 1, prev_signed_block);
    else
        prev_signed_block = *m_pimpl->sync_blocks.rbegin();

    auto header_it = m_pimpl->sync_headers.rbegin() + length;

    if (header_it->prev_hash != meshpp::hash(prev_signed_block.block_details.to_string()))
        throw wrong_data_exception("blockchain response. previous hash!");

    ++header_it;
    for (auto& block_item : response.signed_blocks)
    {
        Block& block = block_item.block_details;
        string str = block.to_string();

        // verify block signature
        if (!meshpp::verify_signature(meshpp::public_key(block_item.authority), str, block_item.signature))
            throw wrong_data_exception("blockchain response. block signature!");

        if (header_it != m_pimpl->sync_headers.rend())
        {
            if (*(header_it - 1) != block.header || header_it->prev_hash != meshpp::hash(str))
                throw wrong_data_exception("blockchain response. block header!");

            ++header_it;
        }

        // verify block transactions
        for (auto tr_it = block.signed_transactions.begin(); tr_it != block.signed_transactions.end(); ++tr_it)
        {
            if (!meshpp::verify_signature(meshpp::public_key(tr_it->authority),
                                          tr_it->transaction_details.to_string(),
                                          tr_it->signature))
                throw wrong_data_exception("blockchain response. transaction signature!");

            if (tr_it->transaction_details.action.type() == Transfer::rtt)
            {
                Transfer transfer;
                tr_it->transaction_details.action.get(transfer);

                if (tr_it->authority != transfer.from)
                    throw wrong_data_exception("blockchain response. transaction authority!");
            }
            else if (tr_it->transaction_details.action.type() == Contract::rtt)
            {
                Contract contract;
                tr_it->transaction_details.action.get(contract);

                if (tr_it->authority != contract.owner)
                    throw wrong_data_exception("blockchain response. transaction authority!");
            }
            else if (tr_it->transaction_details.action.type() == ArticleInfo::rtt)
            {
                ArticleInfo article_info;
                tr_it->transaction_details.action.get(article_info);

                if(tr_it->authority != article_info.channel)
                    throw wrong_data_exception("blockchain response. transaction authority!");
            }
            else if (tr_it->transaction_details.action.type() == StatInfo::rtt)
            {
                // nothing to check here
            }
            else
                throw wrong_data_exception("unknown transaction action type!");

            system_clock::time_point creation = system_clock::from_time_t(tr_it->transaction_details.creation.tm);
            system_clock::time_point expiry = system_clock::from_time_t(tr_it->transaction_details.expiry.tm);

            if (expiry - creation > chrono::hours(TRANSACTION_MAX_LIFETIME_HOURS))
                throw wrong_data_exception("blockchain response. too long lifetime for transaction");

            if (creation - chrono::seconds(NODES_TIME_SHIFT) > system_clock::from_time_t(block.header.time_signed.tm))
                throw wrong_data_exception("blockchain response. transaction from the future!");

            if (expiry < system_clock::from_time_t(block.header.time_signed.tm))
                throw wrong_data_exception("blockchain response. expired transaction!");
        }

        // store blocks for future use
        m_pimpl->sync_blocks.push_back(std::move(block_item));
    }

    // request new chain if needed
    if (m_pimpl->sync_blocks.size() < BLOCK_INSERT_LENGTH &&
        m_pimpl->sync_blocks.size() < m_pimpl->sync_headers.size())
    {
        BlockchainRequest blockchain_request;
        blockchain_request.blocks_from = (header_it - 1)->block_number;
        blockchain_request.blocks_to = m_pimpl->sync_headers.begin()->block_number;

        sk.send(peerid, blockchain_request);
        m_pimpl->update_sync_time();
        m_pimpl->store_request(peerid, blockchain_request);

        return; // will wait new chain
    }

    // test log
    //m_pimpl->writeln_node("applying collected " + std::to_string(m_pimpl->sync_blocks.size()) + " blocks");
    //
    //if(m_pimpl->sync_blocks.size() == 1)
    //    m_pimpl->writeln_node("block mined by " +
    //                          publiqpp::detail::peer_short_names(m_pimpl->sync_blocks.rbegin()->authority));

    //3. all needed blocks received, start to check
    unordered_map<string, system_clock::time_point> transaction_cache_backup = m_pimpl->m_transaction_cache;

    auto now = system_clock::now();
    beltpp::on_failure guard([&m_pimpl, &transaction_cache_backup] 
    { 
        m_pimpl->discard();
        m_pimpl->calc_balance();
        m_pimpl->clear_sync_state(m_pimpl->sync_peerid);
        m_pimpl->m_transaction_cache = transaction_cache_backup;
    });

    vector<string> pool_keys;
    vector<SignedTransaction> pool_transactions;
    m_pimpl->m_transaction_pool.get_keys(pool_keys);
    bool clear_pool = m_pimpl->sync_blocks.size() < m_pimpl->sync_headers.size();

    for (auto& key : pool_keys)
    {
        SignedTransaction signed_transaction;
        m_pimpl->m_transaction_pool.at(key, signed_transaction);

        pool_transactions.push_back(signed_transaction);

        m_pimpl->m_action_log.revert();
        revert_transaction(signed_transaction, m_pimpl, true);

        // This will make sync process faster
        // Most probably removed transactions we will get with blocks
        if (clear_pool)
            m_pimpl->m_transaction_pool.remove(key);
    }

    // complete revert started above
    for(auto& signed_transaction : pool_transactions)
        revert_transaction(signed_transaction, m_pimpl, false);

    // calculate back to get state at LCB point
    block_number = m_pimpl->m_blockchain.length() - 1;
    uint64_t lcb_number = m_pimpl->sync_headers.rbegin()->block_number - 1;
    while (block_number > lcb_number)
    {
        SignedBlock signed_block;
        m_pimpl->m_blockchain.at(block_number, signed_block);
        m_pimpl->m_blockchain.remove_last_block();
        m_pimpl->m_action_log.revert();

        Block& block = signed_block.block_details;

        // decrease all reward amounts from balances and revert reward
        for (auto it = block.rewards.rbegin(); it != block.rewards.rend(); ++it)
            m_pimpl->m_state.decrease_balance(it->to, it->amount);

        // calculate back transactions
        for (auto it = block.signed_transactions.rbegin(); it != block.signed_transactions.rend(); ++it)
        {
            revert_transaction(*it, m_pimpl, true, signed_block.authority);
            revert_transaction(*it, m_pimpl, false, signed_block.authority);

            string key = meshpp::hash((*it).to_string());
            m_pimpl->m_transaction_cache.erase(key);

            if (now <= system_clock::from_time_t(it->transaction_details.expiry.tm))
                m_pimpl->m_transaction_pool.insert(*it); // not yet expired transaction
        }

        --block_number;
    }

    // verify new received blocks
    BlockHeader prev_header;
    m_pimpl->m_blockchain.header_at(lcb_number, prev_header);
    uint64_t c_const = prev_header.c_const;

    //for (auto block_it = m_pimpl->sync_blocks.begin(); block_it != m_pimpl->sync_blocks.end(); ++block_it)
    for (auto const& signed_block : m_pimpl->sync_blocks)
    {
        Block const& block = signed_block.block_details;

        // verify consensus_delta
        Coin amount = m_pimpl->m_state.get_balance(signed_block.authority);
        uint64_t delta = m_pimpl->calc_delta(signed_block.authority, amount.whole, block.header.prev_hash, c_const);
        
        if (delta != block.header.delta)
            throw wrong_data_exception("blockchain response. consensus delta!");

        // verify miner balance at mining time
        if (coin(amount) < MINE_AMOUNT_THRESHOLD)
            throw wrong_data_exception("blockchain response. miner balance!");

        // verify block transactions
        for (auto const& tr_item : block.signed_transactions)
        {
            string key = meshpp::hash(tr_item.to_string());

            m_pimpl->m_transaction_pool.remove(key);

            if (m_pimpl->m_transaction_cache.find(key) != m_pimpl->m_transaction_cache.end())
                throw wrong_data_exception("blockchain response. transaction double use!");

            m_pimpl->m_transaction_cache[key] = system_clock::from_time_t(tr_item.transaction_details.creation.tm);

            if(!apply_transaction(tr_item, m_pimpl, signed_block.authority))
                throw wrong_data_exception("blockchain response. sender balance!");
        }

        // verify block rewards
        if (check_rewards(block, signed_block.authority, m_pimpl))
            throw wrong_data_exception("blockchain response. block rewards!");

        // increase all reward amounts to balances
        for (auto const& reward_item : block.rewards)
            m_pimpl->m_state.increase_balance(reward_item.to, reward_item.amount);

        // Insert to blockchain
        m_pimpl->m_blockchain.insert(signed_block);
        m_pimpl->m_action_log.log_block(signed_block);

        c_const = block.header.c_const;
    }

    m_pimpl->calc_balance();

    // apply back rest of the pool
    m_pimpl->m_transaction_pool.get_keys(pool_keys);

    for (auto& key : pool_keys)
    {
        SignedTransaction signed_transaction;
        m_pimpl->m_transaction_pool.at(key, signed_transaction);

        if(apply_transaction(signed_transaction, m_pimpl))
            m_pimpl->m_action_log.log_transaction(signed_transaction);
        else
            m_pimpl->m_transaction_pool.remove(key);
    }

    m_pimpl->save(guard);
    if (false == m_pimpl->sync_blocks.empty())
        //  please pay special attention to this change during code review
        m_pimpl->calc_sync_info(m_pimpl->sync_blocks.back().block_details);

    // request new chain if the process was stopped
    // by BLOCK_INSERT_LENGTH restriction
    length = m_pimpl->sync_blocks.size();
    if (length < m_pimpl->sync_headers.size())
    {
        // clear already inserted blocks and headers
        m_pimpl->sync_blocks.clear();
        for (size_t i = 0; i < length; ++i)
            m_pimpl->sync_headers.pop_back();

        BlockchainRequest blockchain_request;
        blockchain_request.blocks_from = m_pimpl->sync_headers.rbegin()->block_number;
        blockchain_request.blocks_to = m_pimpl->sync_headers.begin()->block_number;

        sk.send(peerid, blockchain_request);
        m_pimpl->update_sync_time();
        m_pimpl->store_request(peerid, blockchain_request);
    }
    else
    {
        m_pimpl->clear_sync_state(m_pimpl->sync_peerid);

        if (m_pimpl->m_node_type == NodeType::channel)
            broadcast_storage_info(m_pimpl);

        if (m_pimpl->m_node_type == NodeType::storage)
            broadcast_channel_info(m_pimpl);
    }
}

void broadcast_node_type(std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    NodeType my_state_type = m_pimpl->m_state.get_contract_type(m_pimpl->m_pb_key.to_string());

    if (my_state_type == m_pimpl->m_node_type)
        return;

    if (my_state_type != NodeType::miner)
        throw std::runtime_error("Type change is not allowed!");

    bool contract_found = false;
    SignedTransaction signed_transaction;

    vector<string> pool_keys;
    m_pimpl->m_transaction_pool.get_keys(pool_keys);

    for (auto& key : pool_keys)
    {
        m_pimpl->m_transaction_pool.at(key, signed_transaction);

        if (signed_transaction.transaction_details.action.type() == Contract::rtt)
        {
            Contract contract;
            signed_transaction.transaction_details.action.get(contract);

            if (contract.owner == m_pimpl->m_pb_key.to_string())
            {
                contract_found = true;
                break;
            }
        }
    }

    if (!contract_found)
    {
        Contract contract;
        contract.owner = m_pimpl->m_pb_key.to_string();
        contract.role = m_pimpl->m_node_type;

        Transaction transaction;
        transaction.action = contract;
        transaction.creation.tm = system_clock::to_time_t(system_clock::now());
        transaction.expiry.tm = system_clock::to_time_t(system_clock::now() + chrono::hours(24));

        signed_transaction.transaction_details = transaction;
        signed_transaction.authority = m_pimpl->m_pb_key.to_string();
        signed_transaction.signature = m_pimpl->m_pv_key.sign(transaction.to_string()).base58;

        // store to own transaction pool
        process_contract(signed_transaction, contract, m_pimpl);
    }

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

void broadcast_address_info(std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    NodeType my_state_type = m_pimpl->m_state.get_contract_type(m_pimpl->m_pb_key.to_string());

    if (my_state_type == NodeType::miner)
        return;
    if (nullptr == m_pimpl->m_ptr_external_address)
        return;

    AddressInfo address_info;
    address_info.node_id = m_pimpl->m_pb_key.to_string();
    beltpp::assign(address_info.ip_address, *m_pimpl->m_ptr_external_address);

    Transaction transaction;
    transaction.action = address_info;
    transaction.creation.tm = system_clock::to_time_t(system_clock::now());
    transaction.expiry.tm = system_clock::to_time_t(system_clock::now() + chrono::hours(24));

    SignedTransaction signed_transaction;
    signed_transaction.transaction_details = transaction;
    signed_transaction.authority = m_pimpl->m_pb_key.to_string();
    signed_transaction.signature = m_pimpl->m_pv_key.sign(transaction.to_string()).base58;

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

void broadcast_article_info(StorageFileAddress file_address,
                            std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    ArticleInfo article_info;
    article_info.author = "Nikol Pashinyan";
    article_info.content = file_address.uri;
    article_info.channel = m_pimpl->m_pb_key.to_string();

    Transaction transaction;
    transaction.action = article_info;
    transaction.creation.tm = system_clock::to_time_t(system_clock::now());
    transaction.expiry.tm = system_clock::to_time_t(system_clock::now() + chrono::hours(24));

    SignedTransaction signed_transaction;
    signed_transaction.transaction_details = transaction;
    signed_transaction.authority = m_pimpl->m_pb_key.to_string();
    signed_transaction.signature = m_pimpl->m_pv_key.sign(transaction.to_string()).base58;

    if ((process_article_info(signed_transaction, article_info, m_pimpl)))
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

void broadcast_content_info(StorageFileAddress file_address,
                            std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    ContentInfo content_info;
    content_info.content = file_address.uri;
    content_info.storage = m_pimpl->m_pb_key.to_string();

    Transaction transaction;
    transaction.action = content_info;
    transaction.creation.tm = system_clock::to_time_t(system_clock::now());
    transaction.expiry.tm = system_clock::to_time_t(system_clock::now() + chrono::hours(24));

    SignedTransaction signed_transaction;
    signed_transaction.transaction_details = transaction;
    signed_transaction.authority = m_pimpl->m_pb_key.to_string();
    signed_transaction.signature = m_pimpl->m_pv_key.sign(transaction.to_string()).base58;

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

bool process_contract(BlockchainMessage::SignedTransaction const& signed_transaction,
                      BlockchainMessage::Contract const& contract,
                      std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    // Authority check
    if (signed_transaction.authority != contract.owner)
        throw authority_exception(signed_transaction.authority, contract.owner);

    // Don't need to store transaction if sync in process
    // and seems is too far from current block.
    // Just will check the transaction and broadcast
    if (m_pimpl->sync_headers.size() > BLOCK_TR_LENGTH)
        return true;

    // Check pool
    string tr_hash = meshpp::hash(signed_transaction.to_string());

    if (m_pimpl->m_transaction_pool.contains(tr_hash) ||
        m_pimpl->m_transaction_cache.find(tr_hash) != m_pimpl->m_transaction_cache.end())
        return false;

    if (m_pimpl->m_state.get_contract_type(contract.owner) != NodeType::miner)
        return false;

    beltpp::on_failure guard([&m_pimpl] { m_pimpl->discard(); });

    // Validate and add to state
    m_pimpl->m_state.insert_contract(contract);

    // Add to the pool
    m_pimpl->m_transaction_pool.insert(signed_transaction);

    // Add to action log
    m_pimpl->m_action_log.log_transaction(signed_transaction);

    m_pimpl->save(guard);

    return true;
}

bool process_stat_info(BlockchainMessage::SignedTransaction const& signed_transaction,
                       BlockchainMessage::StatInfo const& stat_info,
                       std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    // Check pool and cache
    string tr_hash = meshpp::hash(signed_transaction.to_string());

    if (m_pimpl->m_transaction_pool.contains(tr_hash) ||
        m_pimpl->m_transaction_cache.find(tr_hash) != m_pimpl->m_transaction_cache.end())
        return false;

    // Check data and authority
    NodeType authority_type = m_pimpl->m_state.get_contract_type(signed_transaction.authority);
    
    if (authority_type == NodeType::miner)
        throw wrong_data_exception("wrong authority type : " + signed_transaction.authority);

    for (auto const& item : stat_info.items)
    {
        NodeType item_node_type = m_pimpl->m_state.get_contract_type(item.node);

        if (authority_type == item_node_type)
            throw wrong_data_exception("wrong node type : " + item.node);

        if (item_node_type == NodeType::miner)
            throw wrong_data_exception("wrong node type : " + item.node);
    }

    // Don't need to store transaction if sync in process
    // and seems is too far from current block.
    // Just will check the transaction and broadcast
    if (m_pimpl->sync_headers.size() > BLOCK_TR_LENGTH)
        return true;

    beltpp::on_failure guard([&m_pimpl] { m_pimpl->discard(); });

    // Add to the pool
    m_pimpl->m_transaction_pool.insert(signed_transaction);

    // Add to action log
    m_pimpl->m_action_log.log_transaction(signed_transaction);

    m_pimpl->save(guard);

    return true;
}

bool process_article_info(BlockchainMessage::SignedTransaction const& signed_transaction,
                          BlockchainMessage::ArticleInfo const& article_info,
                          std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    // Check pool and cache
    string tr_hash = meshpp::hash(signed_transaction.to_string());

    if (m_pimpl->m_transaction_pool.contains(tr_hash) ||
        m_pimpl->m_transaction_cache.find(tr_hash) != m_pimpl->m_transaction_cache.end())
        return false;

    // Check data and authority
    if (signed_transaction.authority != article_info.channel)
        throw authority_exception(signed_transaction.authority, article_info.channel);

    NodeType authority_type = m_pimpl->m_state.get_contract_type(signed_transaction.authority);

    if (authority_type != NodeType::channel)
        throw wrong_data_exception("wrong authority type : " + signed_transaction.authority);

    // Don't need to store transaction if sync in process
    // and seems is too far from current block.
    // Just will check the transaction and broadcast
    if (m_pimpl->sync_headers.size() > BLOCK_TR_LENGTH)
        return true;

    beltpp::on_failure guard([&m_pimpl] { m_pimpl->discard(); });

    // Add to the pool
    m_pimpl->m_transaction_pool.insert(signed_transaction);

    // Add to action log
    m_pimpl->m_action_log.log_transaction(signed_transaction);

    m_pimpl->save(guard);

    return true;
}

bool process_content_info(BlockchainMessage::SignedTransaction const& signed_transaction,
                          BlockchainMessage::ContentInfo const& content_info,
                          std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    // Check pool and cache
    string tr_hash = meshpp::hash(signed_transaction.to_string());

    if (m_pimpl->m_transaction_pool.contains(tr_hash) ||
        m_pimpl->m_transaction_cache.find(tr_hash) != m_pimpl->m_transaction_cache.end())
        return false;

    // Check data and authority
    if (signed_transaction.authority != content_info.storage)
        throw authority_exception(signed_transaction.authority, content_info.storage);

    NodeType authority_type = m_pimpl->m_state.get_contract_type(signed_transaction.authority);

    if (authority_type == NodeType::miner)
        throw wrong_data_exception("wrong authority type : " + signed_transaction.authority);

    m_pimpl->m_transaction_cache[tr_hash] = system_clock::from_time_t(signed_transaction.transaction_details.creation.tm);

    if (m_pimpl->m_node_type == NodeType::channel)
    {
        // TODO store content info if needed
    }

    return true;
}

bool process_address_info(BlockchainMessage::SignedTransaction const& signed_transaction,
                          BlockchainMessage::AddressInfo const& address_info,
                          std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    // Check pool and cache
    string tr_hash = meshpp::hash(signed_transaction.to_string());

    if (m_pimpl->m_transaction_pool.contains(tr_hash) ||
        m_pimpl->m_transaction_cache.find(tr_hash) != m_pimpl->m_transaction_cache.end())
        return false;

    // Check data and authority
    if (signed_transaction.authority != address_info.node_id)
        throw authority_exception(signed_transaction.authority, address_info.node_id);

    /*NodeType authority_type = m_pimpl->m_state.get_contract_type(signed_transaction.authority);

    if (authority_type == NodeType::miner)
        throw wrong_data_exception("wrong authority type : " + signed_transaction.authority);*/

    m_pimpl->m_transaction_cache[tr_hash] = system_clock::from_time_t(signed_transaction.transaction_details.creation.tm);

    if (m_pimpl->m_node_type != NodeType::miner)
    {
        beltpp::on_failure guard([&m_pimpl] { m_pimpl->discard(); });

        auto address_info_copy = address_info;
        address_info_copy.ip_address.remote = address_info_copy.ip_address.local;
        address_info_copy.ip_address.local = BlockchainMessage::IPDestination();

        m_pimpl->m_state.update_address(address_info_copy);

        m_pimpl->save(guard);
    }

    return true;
}

//---------------- Exceptions -----------------------
wrong_data_exception::wrong_data_exception(string const& _message)
    : runtime_error("wrong_data_exception -> " + _message)
    , message(_message)
{}
wrong_data_exception::wrong_data_exception(wrong_data_exception const& other) noexcept
    : runtime_error(other)
    , message(other.message)
{}
wrong_data_exception& wrong_data_exception::operator=(wrong_data_exception const& other) noexcept
{
    dynamic_cast<runtime_error*>(this)->operator =(other);
    message = other.message;
    return *this;
}
wrong_data_exception::~wrong_data_exception() noexcept
{}
