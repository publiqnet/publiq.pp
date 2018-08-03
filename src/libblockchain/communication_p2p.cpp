#include "communication_p2p.hpp"

#include "node_internals.hpp"

#include <mesh.pp/cryptoutility.hpp>

#include <map>

using namespace BlockchainMessage;

using std::multimap;

///////////////////////////////////////////////////////////////////////////////////
//                            Internal Finctions

uint64_t calc_delta(string const& key, uint64_t amount, BlockHeader const& block_header)
{
    string key_hash = meshpp::hash(key);

    uint64_t dist = meshpp::distance(key_hash, block_header.previous_hash);
    uint64_t delta = amount / (dist * block_header.consensus_const);

    if (delta > DELTA_MAX)
        delta = DELTA_MAX;

    return delta;
}

void mine_block(unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    SignedBlock prev_signed_block;
    uint64_t block_number = m_pimpl->m_blockchain.length();

    // corner case, managed
    if (block_number == 0)
        m_pimpl->m_blockchain.at(block_number, prev_signed_block);
    else
        m_pimpl->m_blockchain.at(block_number - 1, prev_signed_block);

    beltpp::packet package_prev_block = std::move(prev_signed_block.block_details);
    string prev_block_hash = meshpp::hash(package_prev_block.to_string());

    BlockHeader prev_block_header;
    m_pimpl->m_blockchain.header(prev_block_header);

    string own_key = m_pimpl->private_key.get_public_key().to_string();
    uint64_t amount = m_pimpl->m_state.get_balance(m_pimpl->private_key.get_public_key().to_string());
    uint64_t delta = calc_delta(own_key, amount, prev_block_header);

    // fill new block header data
    BlockHeader block_header;
    block_header.block_number = block_number;
    block_header.consensus_delta = delta;
    block_header.consensus_const = prev_block_header.consensus_const;
    block_header.consensus_sum = prev_block_header.consensus_sum + delta;
    block_header.previous_hash = prev_block_hash;
    block_header.sign_time.tm = system_clock::to_time_t(system_clock::now());

    // update consensus_const if needed
    if (delta > DELTA_UP)
    {
        size_t step = 0;
        BlockHeader prev_header;
        m_pimpl->m_blockchain.header_at(block_number, prev_header);

        while (prev_header.consensus_delta > DELTA_UP &&
            step < DELTA_STEP && prev_header.block_number > 0)
        {
            ++step;
            m_pimpl->m_blockchain.header_at(prev_header.block_number - 1, prev_header);
        }

        if (step >= DELTA_STEP)
            block_header.consensus_const = prev_block_header.consensus_const * 2;
    }
    else
        if (delta < DELTA_DOWN && block_header.consensus_const > 1)
        {
            size_t step = 0;
            BlockHeader prev_header;
            m_pimpl->m_blockchain.header_at(block_number, prev_header);

            while (prev_header.consensus_delta < DELTA_DOWN &&
                step < DELTA_STEP && prev_header.block_number > 0)
            {
                ++step;
                m_pimpl->m_blockchain.header_at(prev_header.block_number - 1, prev_header);
            }

            if (step >= DELTA_STEP)
                block_header.consensus_const = prev_block_header.consensus_const / 2;
        }

    Block block;
    block.header = block_header;

    // grant rewards and move to block
    vector<Reward> rewards;
    m_pimpl->m_transaction_pool.grant_rewards(rewards);

    for (auto& it : rewards)
        block.rewards.push_back(std::move(it));

    // copy transactions from pool to block
    std::vector<std::string> keys;
    m_pimpl->m_transaction_pool.get_keys(keys);

    multimap<BlockchainMessage::ctime, SignedTransaction> transaction_map;

    for (auto& key : keys)
    {
        SignedTransaction signed_transaction;
        m_pimpl->m_transaction_pool.at(key, signed_transaction);

        transaction_map.insert(std::pair<BlockchainMessage::ctime, SignedTransaction>(signed_transaction.transaction_details.creation,
            signed_transaction));
    }

    for (auto it = transaction_map.begin(); it != transaction_map.end(); ++it)
        block.signed_transactions.push_back(std::move(it->second));

    if (!block.signed_transactions.empty() || !block.rewards.empty())
    {
        // grant miner reward himself
        Reward own_reward;
        own_reward.amount = MINER_REWARD;
        own_reward.to = own_key;

        block.rewards.push_back(own_reward);
    }

    // sign block and insert to blockchain
    meshpp::signature sgn = m_pimpl->private_key.sign(block.to_string());

    SignedBlock signed_block;
    signed_block.signature = sgn.base64;
    signed_block.authority = sgn.pb_key.to_string();
    signed_block.block_details = std::move(block);

    vector<SignedBlock> signed_block_vector;
    signed_block_vector.push_back(signed_block);

    insert_blocks(signed_block_vector, m_pimpl);
}

bool insert_blocks(vector<SignedBlock>& signed_block_vector,
                   unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    std::unordered_set<string> erase_tpool_set;
    std::unordered_map<string, uint64_t> tmp_state;
    std::vector<LoggedTransaction> logged_transactions;
    std::vector<std::pair<std::string, uint64_t>> amounts;

    for (auto it = signed_block_vector.begin(); it != signed_block_vector.end(); ++it)
    {
        uint64_t fee = 0;
        SignedBlock signed_block = *it;

        Block block;
        std::move(signed_block.block_details).get(block);

        // Check block transactions and calculate new state
        for (auto &signed_transaction : block.signed_transactions)
        {
            Transaction transaction = std::move(signed_transaction.transaction_details);

            Transfer transfer;
            transaction.action.get(transfer);

            if (signed_transaction.authority != transfer.from)
                return false;

            string key;
            uint64_t amount;

            // correct "from" key balance
            key = transfer.from;
            if(tmp_state.find(key) == tmp_state.end())
            {
                tmp_state[key] = m_pimpl->m_state.get_balance(key);

                // process "key" output transfers
                amounts.clear();
                m_pimpl->m_transaction_pool.get_amounts(key, amounts, false);

                amount = tmp_state[key];
                for (auto& it : amounts)
                {
                    amount += it.second;
                    erase_tpool_set.insert(it.first);
                }

                // process "key" input transfers
                amounts.clear();
                m_pimpl->m_transaction_pool.get_amounts(key, amounts, true);

                for (auto& it : amounts)
                {
                    if (amount >= it.second)
                    {
                        amount -= it.second;
                        erase_tpool_set.insert(it.first);
                    }
                    else
                        return false;
                }
                tmp_state[key] = amount;
            }

            // remove transfer amount and fee from sender balance
            amount = tmp_state[key];
            if (amount >= transfer.amount + transaction.fee)
            {
                tmp_state[key] = amount - transfer.amount - transaction.fee;
                fee += transaction.fee;
            }
            else
                return false;

            // correct to_key balance
            key = transfer.to;
            if (tmp_state.find(key) == tmp_state.end())
            {
                tmp_state[key] = m_pimpl->m_state.get_balance(key);

                // process "key" output transfers
                amounts.clear();
                m_pimpl->m_transaction_pool.get_amounts(key, amounts, false);

                amount = tmp_state[key];
                for (auto& it : amounts)
                {
                    amount += it.second;
                    erase_tpool_set.insert(it.first);
                }

                // process "key" input transfers
                amounts.clear();
                m_pimpl->m_transaction_pool.get_amounts(key, amounts, true);

                for (auto& it : amounts)
                {
                    if (amount >= it.second)
                    {
                        amount -= it.second;
                        erase_tpool_set.insert(it.first);
                    }
                    else
                        return false;
                }
                tmp_state[key] = amount;
            }

            // add transfer amount to receiver balance
            amount = tmp_state[key];
            tmp_state[key] = amount + transfer.amount;

            // collect action log
            LoggedTransaction action_info;
            action_info.applied_reverted = true;
            action_info.index = 0;
            action_info.action = std::move(transaction);
            logged_transactions.push_back(std::move(action_info));
        }

        // apply rewards to tmp_state
        for (auto& reward : block.rewards)
        {
            uint64_t amount = 0;
            string key = reward.to;

            auto state_it = tmp_state.find(key);
            if (state_it != tmp_state.end())
                amount = state_it->second;
            else
                amount = m_pimpl->m_state.get_balance(key);

            tmp_state[key] = amount + reward.amount;
        }

        // add fee to miner balance
        uint64_t amount = 0;
        auto state_it = tmp_state.find(signed_block.authority);
        if (state_it != tmp_state.end())
            amount = state_it->second;
        else
            amount = m_pimpl->m_state.get_balance(signed_block.authority);

        tmp_state[signed_block.authority] = amount + fee;
    }

    // Insert blocks
    for (auto it = signed_block_vector.begin(); it != signed_block_vector.end(); ++it)
        m_pimpl->m_blockchain.insert(std::move(*it));

    // Correct state
    m_pimpl->m_state.merge_block(tmp_state);

    // Correct action log ( 1. revert old action log )
    size_t count = m_pimpl->m_transaction_pool.length();
    for (size_t i = 0; i < count; ++i)
        m_pimpl->m_action_log.revert();

    // Correct action log ( 2. apply block transfers )
    for (auto &it : logged_transactions)
        m_pimpl->m_action_log.insert(it);

    // Correct action log ( 3. apply rest of transaction pool )
    for (auto &it : erase_tpool_set)
        m_pimpl->m_transaction_pool.remove(it);

    std::vector<std::string> keys;
    m_pimpl->m_transaction_pool.get_keys(keys);

    auto now = system_clock::now();
    system_clock::to_time_t(now);

    for (auto &it : keys)
    {
        SignedTransaction signed_transaction;
        m_pimpl->m_transaction_pool.at(it, signed_transaction);

        if (now > system_clock::from_time_t(signed_transaction.transaction_details.expiry.tm))
        {
            LoggedTransaction action_info;
            action_info.applied_reverted = true;
            action_info.index = 0;
            action_info.action = std::move(signed_transaction.transaction_details.action);

            m_pimpl->m_action_log.insert(action_info);
        }
    }

    return true;
}

void revert_blocks(size_t count, 
                   std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    std::unordered_map<string, uint64_t> tmp_state;

    for (size_t i = 0; i < count; ++i)
    {
        uint64_t number = m_pimpl->m_blockchain.length() - 1;

        SignedBlock signed_block;
        m_pimpl->m_blockchain.at(number, signed_block);

        Block block;
        std::move(signed_block.block_details).get(block);

        auto it = block.signed_transactions.begin();

        // Add block transactions to the pool
        for (; it != block.signed_transactions.end(); ++it)
            m_pimpl->m_transaction_pool.insert(*it);

        // Remove last block from blockchain
        m_pimpl->m_blockchain.remove_last_block();

        // Correct state, remove block rewards
        for (auto& reward : block.rewards)
        {
            uint64_t amount = 0;
            string key = reward.to;

            auto it = tmp_state.find(key);
            if (it != tmp_state.end())
                amount = it->second;
            else
                amount = m_pimpl->m_state.get_balance(key);

            // txur klni ete amount < reward.amount ;)
            tmp_state[key] = amount - reward.amount;
        }
    }

    // Correct state
    m_pimpl->m_state.merge_block(tmp_state);

    // Action log should be correct :)
}

///////////////////////////////////////////////////////////////////////////////////

void process_sync_request(beltpp::packet& package,
                          std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                          beltpp::isocket& sk,
                          beltpp::isocket::peer_id const& peerid)
{
    SyncRequest sync_request;
    std::move(package).get(sync_request);

    BlockHeader block_header;
    m_pimpl->m_blockchain.header(block_header);

    SyncResponse sync_response;
    sync_response.block_number = block_header.block_number;
    sync_response.consensus_sum = block_header.consensus_sum;

    if (sync_response.block_number > sync_request.block_number ||
        (sync_response.block_number == sync_request.block_number &&
            sync_response.consensus_sum > sync_request.consensus_sum))
    {
        sk.send(peerid, std::move(sync_response));
    }
}

void process_blockheader_request(beltpp::packet& package,
                                 std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                                 beltpp::isocket& sk,
                                 beltpp::isocket::peer_id const& peerid)
{
    // headers always requested in reverse order!
    BlockHeaderRequest header_request;
    std::move(package).get(header_request);

    uint64_t from = m_pimpl->m_blockchain.length() - 1;
    from = from < header_request.blocks_from ? from : header_request.blocks_from;

    uint64_t to = header_request.blocks_to;
    to = to > from ? from : to;
    to = from > TRANSFER_LENGTH && to < from - TRANSFER_LENGTH ? from - TRANSFER_LENGTH : to;

    BlockHeaderResponse header_response;
    for (auto index = from; index >= to;)
    {
        BlockHeader header;
        m_pimpl->m_blockchain.header_at(index, header);

        header_response.block_headers.push_back(std::move(header));

        if (index > 0)
            --index;
        else
            break;
    }

    sk.send(peerid, header_response);
}

void process_blockheader_response(beltpp::packet& package,
                                  std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                                  beltpp::isocket& sk,
                                  beltpp::isocket::peer_id const& peerid)
{
    // find needed header from own data
    BlockHeader tmp_header;
    m_pimpl->m_blockchain.header(tmp_header);

    BlockHeaderResponse header_response;
    std::move(package).get(header_response);

    // validate received headers
    auto it = header_response.block_headers.begin();
    bool bad_data = header_response.block_headers.empty();

    if (!m_pimpl->sync_header_vector.empty() && // we have something received before
        tmp_header.block_number >= m_pimpl->sync_header_vector.rbegin()->block_number)
    {
        // load next mot checked header
        m_pimpl->m_blockchain.header_at(m_pimpl->sync_header_vector.rbegin()->block_number - 1, tmp_header);

        bad_data = bad_data || tmp_header.block_number != (*it).block_number;
    }

    if (bad_data)
        throw wrong_data_exception("process_blockheader_response. empty data in header!");

    if (m_pimpl->sync_header_vector.empty())
    {
        system_clock::time_point time_point1 = system_clock::from_time_t((*(it)).sign_time.tm);
        system_clock::time_point time_point2 = system_clock::from_time_t(tmp_header.sign_time.tm);

        bad_data = bad_data || time_point1 < time_point2;

        chrono::seconds diff_seconds = chrono::duration_cast<chrono::seconds>(time_point1 - time_point2);

        bad_data = bad_data || diff_seconds.count() < BLOCK_MINE_DELAY;
    }

    if (bad_data)
        throw wrong_data_exception("process_blockheader_response. incorrect data in header!");

    for (++it; !bad_data && it != header_response.block_headers.end(); ++it)
    {
        bad_data = bad_data || (*(it - 1)).block_number != (*it).block_number + 1;
        bad_data = bad_data || (*(it - 1)).consensus_sum <= (*it).consensus_sum;
        bad_data = bad_data || (*(it - 1)).consensus_sum != (*(it - 1)).consensus_delta + (*it).consensus_sum;
        bad_data = bad_data || (
                                (*(it - 1)).consensus_const != (*it).consensus_const &&
                                (*(it - 1)).consensus_const != 2 * (*it).consensus_const
                               );
        
        system_clock::time_point time_point1 = system_clock::from_time_t((*(it)).sign_time.tm);
        system_clock::time_point time_point2 = system_clock::from_time_t((*(it - 1)).sign_time.tm);

        bad_data = bad_data || time_point1 > time_point2;

        chrono::seconds diff_seconds = chrono::duration_cast<chrono::seconds>(time_point2 - time_point1);

        bad_data = bad_data || diff_seconds.count() < BLOCK_MINE_DELAY;
    }

    if (bad_data)
        throw wrong_data_exception("process_blockheader_response. incorrect data in header!");

    // find last common header
    bool found = false;
    it = header_response.block_headers.begin();
    while (!found && it != header_response.block_headers.end())
    {
        if (tmp_header.block_number < (*it).block_number)
        {
            // store for possible use
            m_pimpl->sync_header_vector.push_back(std::move(*it));
            ++it;
        }
        else
            found = true;
    }

    bool lcb_found = false;
    if (found)
    {
        for (; !lcb_found && it != header_response.block_headers.end(); ++it)
        {
            if (tmp_header.previous_hash == it->previous_hash)
            {
                lcb_found = true;
                break;
            }

            if (it->block_number == 0) // remove after genesis made
                lcb_found = true;

            // store for possible use
            m_pimpl->sync_header_vector.push_back(std::move(*it));
            m_pimpl->m_blockchain.header_at(tmp_header.block_number - 1, tmp_header);
        }

        if (lcb_found)
        {
            bad_data = m_pimpl->sync_header_vector.empty();

            //// verify consensus_const
            //if (!bad_data)
            //{
            //    vector<pair<uint64_t, uint64_t>> delta_vector;
            //
            //    for (auto it = m_pimpl->sync_header_vector.begin(); it != m_pimpl->sync_header_vector.end(); ++it)
            //        delta_vector.push_back(pair<uint64_t, uint64_t>(it->consensus_delta, it->consensus_const));
            //
            //    uint64_t number = m_pimpl->sync_header_vector.rbegin()->block_number - 1;
            //    uint64_t delta_step = number < DELTA_STEP ? number : DELTA_STEP;
            //
            //    for (uint64_t i = 0; i < delta_step; ++i)
            //    {
            //        BlockHeader block_header;
            //        m_pimpl->m_blockchain.header_at(number - i, block_header);
            //
            //        delta_vector.push_back(pair<uint64_t, uint64_t>(block_header.consensus_delta, block_header.consensus_const));
            //    }
            //
            //    for (auto it = delta_vector.begin(); !bad_data && it + delta_step != delta_vector.end(); ++it)
            //    {
            //        if (it->first > DELTA_UP)
            //        {
            //            size_t step = 0;
            //            uint64_t _delta = it->first;
            //
            //            while (_delta > DELTA_UP && step < DELTA_STEP && it + step != delta_vector.end())
            //            {
            //                ++step;
            //                _delta = (it + step)->first;
            //            }
            //
            //            if (step >= DELTA_STEP)
            //                bad_data = it->second != (it + 1)->second * 2;
            //        }
            //        else if (it->first < DELTA_DOWN && it->second > 1)
            //        {
            //            size_t step = 0;
            //            uint64_t _delta = it->first;
            //
            //            while (_delta < DELTA_DOWN && step < DELTA_STEP && it + step != delta_vector.end())
            //            {
            //                ++step;
            //                _delta = (it + step)->first;
            //            }
            //
            //            if (step >= DELTA_STEP)
            //                bad_data = it->second != (it + 1)->second / 2;
            //        }
            //    }
            //}
            //
            //if (bad_data)
            //    throw wrong_data_exception("process_blockheader_response. nothing new!");

            //3. request blockchain from found point
            BlockChainRequest blockchain_request;
            blockchain_request.blocks_from = m_pimpl->sync_header_vector.rbegin()->block_number;
            blockchain_request.blocks_to = m_pimpl->sync_header_vector.begin()->block_number;

            sk.send(peerid, blockchain_request);
            m_pimpl->update_sync_time();
            m_pimpl->store_request(peerid, blockchain_request);
        }
    }

    if (!found || !lcb_found)
    {
        // request more headers
        BlockHeaderRequest header_request;
        header_request.blocks_from = m_pimpl->sync_header_vector.rbegin()->block_number - 1;
        header_request.blocks_to = header_request.blocks_from > TRANSFER_LENGTH ? header_request.blocks_from - TRANSFER_LENGTH : 0;

        sk.send(peerid, header_request);
        m_pimpl->update_sync_time();
        m_pimpl->store_request(peerid, header_request);
    }
}

void process_blockchain_request(beltpp::packet& package,
                                std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                                beltpp::isocket& sk,
                                beltpp::isocket::peer_id const& peerid)
{
    // blocks are always requested in regular order
    BlockChainRequest blockchain_request;
    std::move(package).get(blockchain_request);

    uint64_t number = m_pimpl->m_blockchain.length() - 1;
    uint64_t from = number < blockchain_request.blocks_from ? number : blockchain_request.blocks_from;

    uint64_t to = blockchain_request.blocks_to;
    to = to < from ? from : to;
    to = to > from + TRANSFER_LENGTH ? from + TRANSFER_LENGTH : to;
    to = to > number ? number : to;

    BlockChainResponse chain_response;
    for (auto i = from; i <= to; ++i)
    {
        SignedBlock signed_block;
        m_pimpl->m_blockchain.at(i, signed_block);

        chain_response.signed_blocks.push_back(std::move(signed_block));
    }

    sk.send(peerid, chain_response);
}

void process_blockchain_response(beltpp::packet& package,
                                 std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                                 beltpp::isocket& sk,
                                 beltpp::isocket::peer_id const& peerid)
{
    //1. check received blockchain validity
    BlockChainResponse response;
    std::move(package).get(response);

    bool bad_data = response.signed_blocks.empty() ||
                    response.signed_blocks.size() > m_pimpl->sync_header_vector.size() -
                                                    m_pimpl->sync_block_vector.size();

    if (bad_data)
        throw wrong_data_exception("process_blockheader_response. zibil en dayax arel!");

    // find last common block
    uint64_t block_number = (*m_pimpl->sync_header_vector.rbegin()).block_number;

    //if (block_number == 0)
    //    throw wrong_data_exception("process_blockheader_response. uzum en qcen!");

    SignedBlock prev_signed_block;
    if(block_number > 0) //corner if
    m_pimpl->m_blockchain.at(block_number - 1, prev_signed_block);

    Block prev_block;
    if (block_number > 0) //corner if
    std::move(prev_signed_block.block_details).get(prev_block);

    std::unordered_map<string, uint64_t> accounts_diff;
    if (block_number > 0) //corner if
    block_number = m_pimpl->m_blockchain.length() - 1;

    //-----------------------------------------------------//
    auto get_balance = [&](string& key)
    {
        if (accounts_diff.find(key) != accounts_diff.end())
            return accounts_diff[key];
        else
            return m_pimpl->m_state.get_balance(key);
    };

    auto increase_balance = [&](string& key, uint64_t amount)
    {
        accounts_diff[key] = get_balance(key) + amount;
    };

    auto decrease_balance = [&](string& key, uint64_t amount)
    {
        auto balance = get_balance(key);

        if (balance < amount)
            return false;

        accounts_diff[key] = balance - amount;

        return true;
    };
    //-----------------------------------------------------//

    // calculate back to get accounts_diff at LCB point
    if (block_number > 0) //corner if
    while (block_number > prev_block.header.block_number)
    {
        SignedBlock signed_block;
        m_pimpl->m_blockchain.at(block_number, signed_block);

        Block block;
        std::move(signed_block.block_details).get(block);
        
        // decrease all reward amounts from balances
        for (auto it = block.rewards.begin(); it != block.rewards.end(); ++it)
            decrease_balance(it->to, it->amount);

        // calculate back transactions
        uint64_t fee = 0;
        for (auto it = block.signed_transactions.rbegin(); it != block.signed_transactions.rend(); ++it)
        {
            fee += it->transaction_details.fee;

            Transfer transfer;
            std::move(it->transaction_details.action).get(transfer);

            // revert sender balance
            increase_balance(transfer.from, transfer.amount + it->transaction_details.fee);

            // revert receiver balance, hope no problems here ;)
            decrease_balance(transfer.to, transfer.amount);
        }

        // revert authority balance, hope no problems here ;)
        decrease_balance(signed_block.authority, fee);

        --block_number;
    }

    // apply sync_block_vector content to accounts_diff
    for(auto it = m_pimpl->sync_block_vector.begin(); it != m_pimpl->sync_block_vector.end(); ++it)
    {
        Block block;
        it->block_details.get(block);

        // calculate transactions
        uint64_t fee = 0;
        for (auto it = block.signed_transactions.begin(); it != block.signed_transactions.end(); ++it)
        {
            fee += it->transaction_details.fee;

            Transfer transfer;
            std::move(it->transaction_details.action).get(transfer);

            // calc sender balance, hope no problems here ;)
            decrease_balance(transfer.from, transfer.amount + it->transaction_details.fee);

            // calc receiver balance
            increase_balance(transfer.to, transfer.amount);
        }

        // calc authority balance
        increase_balance(it->authority, fee);

        // increase all reward amounts to balances
        for (auto it = block.rewards.begin(); it != block.rewards.end(); ++it)
            increase_balance(it->to, it->amount);
    }

    // put prev_block in correct place
    if (!m_pimpl->sync_block_vector.empty())
    {
        prev_signed_block = *m_pimpl->sync_block_vector.rbegin();
        prev_signed_block.block_details.get(prev_block);
    }

    BlockHeader own_header;
    m_pimpl->m_blockchain.header(own_header);

    // verify new received blocks
    for (auto block_it = response.signed_blocks.begin(); block_it != response.signed_blocks.end(); ++block_it)
    {
        // verify block signature
        bool sb_verify = meshpp::verify_signature(meshpp::public_key(block_it->authority),
                                                  block_it->block_details.to_string(),
                                                  block_it->signature);
        
        bad_data = !sb_verify;
        if (bad_data) break;

        Block block;
        block_it->block_details.get(block);

        // hankarc sxal headernerov xapac chlnen skzbic
        if (own_header.block_number == block.header.block_number)
        {
            bad_data = own_header.consensus_sum > block.header.consensus_sum;
        }
        if (bad_data) break;

        // verify block number
        if (block_number > 0) //corner if
        bad_data = block.header.block_number != prev_block.header.block_number + 1;
        if (bad_data) break;

        // verify previous_hash
        if (block_number > 1) //corner if
        bad_data = block.header.previous_hash != meshpp::hash(prev_block.to_string());
        if (bad_data) break;

        // verify consensus_delta
        uint64_t amount = get_balance(block_it->authority);
        //if (block_number > 1) //corner if
        //{
        //    uint64_t delta = calc_delta(block_it->authority, amount, prev_block.header);
        //
        //    bad_data = delta != block.header.consensus_delta;
        //    if (bad_data) break;
        //}

        // verify consensus_sum
        if (block_number > 1) //corner if
        bad_data = block.header.consensus_sum != (block.header.consensus_delta + prev_block.header.consensus_sum);
        if (bad_data) break;

        // verify miner balance at mining point
        if (block_number > 1) //corner if
        bad_data = amount < MINE_AMOUNT_THRESHOLD;
        if (bad_data) break;

        // verify block transactions
        uint64_t fee = 0;
        for (auto tr_it = block.signed_transactions.begin(); tr_it != block.signed_transactions.end(); ++tr_it)
        {
            bool st_verify = meshpp::verify_signature(meshpp::public_key(tr_it->authority),
                                                      tr_it->transaction_details.to_string(),
                                                      tr_it->signature);
            
            bad_data = !st_verify;
            if (bad_data) break;

            Transfer transfer;
            tr_it->transaction_details.action.get(transfer);

            bad_data = tr_it->authority != transfer.from;
            if (bad_data) break;

            fee += tr_it->transaction_details.fee;

            // decrease "from" balance
            bad_data = !decrease_balance(transfer.from, transfer.amount + tr_it->transaction_details.fee);
            if (bad_data) break;

            // increase "to" balance
            increase_balance(transfer.to, transfer.amount);
        }

        if (bad_data) break;

        // increase authority balance
        increase_balance(block_it->authority, fee);

        //TODO verify rewards!

        // increase all reward amounts to balances
        for (auto reward_it = block.rewards.begin(); reward_it != block.rewards.end(); ++reward_it)
            increase_balance(reward_it->to, reward_it->amount);

        prev_block = std::move(block);
        ++block_number; // corner
    }

    if (bad_data) 
        throw wrong_data_exception("process_blockheader_response. zibil en dayax arel!");

    //2. add received blockchain to sync_blocks_vector for future process
    for (auto it = response.signed_blocks.begin(); it != response.signed_blocks.end(); ++it)
        m_pimpl->sync_block_vector.push_back(std::move(*it));

    // request new chain if needed
    if (m_pimpl->sync_block_vector.size() < m_pimpl->sync_header_vector.size())
    {
        BlockChainRequest blockchain_request;
        blockchain_request.blocks_from = prev_block.header.block_number + 1;
        blockchain_request.blocks_to = m_pimpl->sync_header_vector.begin()->block_number;

        sk.send(peerid, blockchain_request);
        m_pimpl->update_sync_time();
        m_pimpl->store_request(peerid, blockchain_request);

        return; // will wait new chain
    }

    //3. apply received chain
    vector<SignedBlock> revert_block_vector;
    uint64_t from = m_pimpl->sync_header_vector.rbegin()->block_number;
    if(m_pimpl->m_blockchain.length() > 0) // corner if
    for (auto i = m_pimpl->m_blockchain.length() - 1; i >= from;)
    {
        SignedBlock signed_block;
        m_pimpl->m_blockchain.at(i, signed_block);

        revert_block_vector.push_back(std::move(signed_block));

        if (i > 0)
            --i;
        else
            break;
    }

    revert_blocks(m_pimpl->m_blockchain.length() - from, m_pimpl);

    if(!insert_blocks(m_pimpl->sync_block_vector, m_pimpl))
        if (!insert_blocks(revert_block_vector, m_pimpl))
            throw std::runtime_error("Something wrong happenes. Cant't insert back own chain!");
}

//---------------- Exceptions -----------------------
wrong_data_exception::wrong_data_exception(string const& _message)
    : runtime_error("Zibil en uxxarkel! message: " + _message)
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
