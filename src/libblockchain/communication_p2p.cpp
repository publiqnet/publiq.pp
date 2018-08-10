#include "communication_p2p.hpp"
#include "common.hpp"

#include "node_internals.hpp"
#include "coin.hpp"

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

bool insert_blocks(vector<SignedBlock>& signed_blocks,
                   unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    if (signed_blocks.size() == 0)
        return true;

    vector<LoggedTransaction> actions;
    vector<pair<string, coin>> amounts;
    unordered_set<string> erase_tpool;
    unordered_map<string, Coin> tmp_state;

    //-----------------------------------------------------//
    auto get_balance = [&](string& key)
    {
        auto state_it = tmp_state.find(key);
        if (state_it != tmp_state.end())
            return state_it->second;
        else
            return m_pimpl->m_state.get_balance(key);
    };

    auto increase_balance = [&](string& key, coin const& amount)
    {
        if (amount.empty()) 
            return;

        auto balance = get_balance(key);

        tmp_state[key] = (balance + amount).to_Coin();
    };
    
    auto decrease_balance = [&](string& key, coin const& amount)
    {
        if (amount.empty()) 
            return true;

        auto balance = get_balance(key);
    
        if (coin(balance) < amount)
            return false;
    
        tmp_state[key] = (balance - amount).to_Coin();
    
        return true;
    };

    auto log_transaction = [&](beltpp::packet action)
    {
        LoggedTransaction action_info;
        action_info.applied_reverted = true;
        action_info.index = 0;
        action_info.action = std::move(action);

        actions.push_back(std::move(action_info));
    };
    //-----------------------------------------------------//

    for (auto it = signed_blocks.begin(); it != signed_blocks.end(); ++it)
    {
        coin fee;
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

            // correct "from" key balance
            if (tmp_state.find(transfer.from) == tmp_state.end())
            {
                // process "key" output transfers
                m_pimpl->m_transaction_pool.get_amounts(transfer.from, amounts, false);

                for (auto& item : amounts)
                {
                    increase_balance(transfer.from, item.second);
                    erase_tpool.insert(item.first);
                }

                // process "key" input transfers
                m_pimpl->m_transaction_pool.get_amounts(transfer.from, amounts, true);

                for (auto& item : amounts)
                {
                    if (decrease_balance(transfer.from, item.second))
                        erase_tpool.insert(item.first);
                    else
                        return false;
                }
            }

            // remove transfer amount and fee from sender balance
            if(decrease_balance(transfer.from, transfer.amount + transaction.fee))
                fee += transaction.fee;
            else
                return false;

            // correct to_key balance
            if (tmp_state.find(transfer.to) == tmp_state.end())
            {
                // process "key" output transfers
                m_pimpl->m_transaction_pool.get_amounts(transfer.to, amounts, false);

                for (auto& item : amounts)
                {
                    increase_balance(transfer.to, item.second);
                    erase_tpool.insert(item.first);
                }

                // process "key" input transfers
                m_pimpl->m_transaction_pool.get_amounts(transfer.to, amounts, true);

                for (auto& item : amounts)
                {
                    if (decrease_balance(transfer.to, item.second))
                        erase_tpool.insert(item.first);
                    else
                        return false;
                }
            }

            // add transfer amount to receiver balance
            increase_balance(transfer.to, transfer.amount);

            // collect action log
            log_transaction(std::move(transfer));
        }

        // add fee to miner balance
        increase_balance(signed_block.authority, fee);

        //TODO manage fee

        // apply rewards to tmp_state
        for (auto& reward : block.rewards)
        {
            increase_balance(reward.to, reward.amount);

            // collect action log
            log_transaction(std::move(reward));
        }
    }

    // return false is not allowed after this point!

    // Insert blocks
    for (auto it = signed_blocks.begin(); it != signed_blocks.end(); ++it)
        m_pimpl->m_blockchain.insert(std::move(*it));

    // Correct state
    m_pimpl->m_state.merge_block(tmp_state);

    // Correct action log ( 1. revert old action log )
    size_t count = m_pimpl->m_transaction_pool.length();
    for (size_t i = 0; i < count; ++i)
        m_pimpl->m_action_log.revert();

    // Correct action log ( 2. apply block transfers )
    for (auto& action : actions)
        m_pimpl->m_action_log.insert(action);

    actions.clear();

    // Correct action log ( 3. apply rest of transaction pool )
    for (auto& item : erase_tpool)
        m_pimpl->m_transaction_pool.remove(item);

    vector<string> keys;
    m_pimpl->m_transaction_pool.get_keys(keys);

    auto now = system_clock::now();
    system_clock::to_time_t(now);

    for (auto& key : keys)
    {
        SignedTransaction signed_transaction;
        m_pimpl->m_transaction_pool.at(key, signed_transaction);

        if (now > system_clock::from_time_t(signed_transaction.transaction_details.expiry.tm))
            log_transaction(std::move(signed_transaction.transaction_details.action));
        else
            m_pimpl->m_transaction_pool.remove(key);
    }

    // Correct action log. apply pool valid transactions
    for (auto& action : actions)
        m_pimpl->m_action_log.insert(action);

    return true;
}

void revert_blocks(size_t count,
                   unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                   vector<SignedBlock>& backup_blocks)
{
    if (count == 0)
        return;

    if (count >= m_pimpl->m_blockchain.length())
        throw std::runtime_error("Serious bug in algorithm!");

    // revert transaction pool content from action log
    vector<string> keys;
    vector<beltpp::packet> actions;
    m_pimpl->m_transaction_pool.get_keys(keys);

    for (auto& key : keys)
    {
        // revert from action_log
        m_pimpl->m_action_log.revert();

        // store actions for insert to action_log
        SignedTransaction signed_transaction;
        m_pimpl->m_transaction_pool.at(key, signed_transaction);

        actions.push_back(std::move(signed_transaction.transaction_details.action));
    }

    unordered_map<string, BlockchainMessage::Coin> tmp_state;

    for (size_t i = 0; i < count; ++i)
    {
        SignedBlock signed_block;
        m_pimpl->m_blockchain.at(m_pimpl->m_blockchain.length() - 1, signed_block);

        // bachup and return for rollback
        backup_blocks.push_back(signed_block);

        Block block;
        std::move(signed_block.block_details).get(block);

        // Correct state, remove block rewards
        for (auto& reward : block.rewards)
        {
            // revert from action_log
            m_pimpl->m_action_log.revert();

            // correct state
            coin balance;

            auto it = tmp_state.find(reward.to);
            if (it != tmp_state.end())
                balance = it->second;
            else
                balance = m_pimpl->m_state.get_balance(reward.to);

            if (balance >= reward.amount)
                tmp_state[reward.to] = (balance - reward.amount).to_Coin();
            else
                throw std::runtime_error("It's a catastrophe!");
        }

        // Add block transactions to the pool and revert from action_log
        for (auto it = block.signed_transactions.rbegin(); it != block.signed_transactions.rend(); ++it)
        {
            m_pimpl->m_action_log.revert();
            m_pimpl->m_transaction_pool.insert(*it);

            actions.push_back(std::move(it->transaction_details.action));
        }

        // Remove last block from blockchain
        m_pimpl->m_blockchain.remove_last_block();
    }

    // Correct state
    m_pimpl->m_state.merge_block(tmp_state);

    // Insert actions to action_log
    for (auto it = actions.rbegin(); it != actions.rend(); ++it)
    {
        LoggedTransaction action_info;
        action_info.applied_reverted = true;
        action_info.index = 0;
        action_info.action = std::move(*it);

        m_pimpl->m_action_log.insert(std::move(action_info));
    }
}

bool check_headers(BlockHeader const& next_header, BlockHeader const& header)
{
    bool t = next_header.block_number != header.block_number + 1;
    t = t || next_header.consensus_sum <= header.consensus_sum;
    t = t || next_header.consensus_sum != next_header.consensus_delta + header.consensus_sum;
    t = t || (next_header.consensus_const != header.consensus_const &&
              next_header.consensus_const != 2 * header.consensus_const);

    system_clock::time_point time_point1 = system_clock::from_time_t(header.sign_time.tm);
    system_clock::time_point time_point2 = system_clock::from_time_t(next_header.sign_time.tm);
    chrono::seconds diff_seconds = chrono::duration_cast<chrono::seconds>(time_point2 - time_point1);

    return t || time_point1 > time_point2 || diff_seconds.count() < BLOCK_MINE_DELAY;
};

///////////////////////////////////////////////////////////////////////////////////

void insert_genesis(std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    if (m_pimpl->m_blockchain.length() > 0)
        return;

    BlockHeader block_header;
    block_header.block_number = 0;
    block_header.consensus_sum = 0;
    block_header.consensus_delta = 0;
    block_header.consensus_const = 1;
    block_header.previous_hash = meshpp::hash("Ice Age");
    block_header.sign_time.tm = 0;

    Block block;
    block.header = block_header;

    meshpp::random_seed node_rs("NODE");
    meshpp::private_key node_pv = node_rs.get_private_key(0);

    Reward reward;
    reward.amount = coin(100, 0).to_Coin();
    reward.to = node_pv.get_public_key().to_string();
    block.rewards.push_back(std::move(reward));

    meshpp::random_seed rs("GENESIS");
    meshpp::private_key pv = rs.get_private_key(0);
    meshpp::signature sgn = pv.sign(block.to_string());

    SignedBlock signed_block;
    signed_block.signature = sgn.base64;
    signed_block.authority = node_pv.get_public_key().to_string();
    signed_block.block_details = std::move(block);

    vector<SignedBlock> signed_block_vector;
    signed_block_vector.push_back(signed_block);

    insert_blocks(signed_block_vector, m_pimpl);
}

void mine_block(unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    SignedBlock prev_signed_block;
    uint64_t block_number = m_pimpl->m_blockchain.length();
    m_pimpl->m_blockchain.at(block_number - 1, prev_signed_block);

    beltpp::packet package_prev_block = std::move(prev_signed_block.block_details);
    string prev_block_hash = meshpp::hash(package_prev_block.to_string());

    BlockHeader prev_block_header;
    m_pimpl->m_blockchain.header(prev_block_header);

    string own_key = m_pimpl->private_key.get_public_key().to_string();
    coin amount = m_pimpl->m_state.get_balance(m_pimpl->private_key.get_public_key().to_string());
    uint64_t delta = calc_delta(own_key, amount.to_uint64_t(), prev_block_header);

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
    vector<string> keys;
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
        own_reward.amount = MINER_REWARD.to_Coin();
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
    for (auto index = from + 1; index > to; --index)
    {
        BlockHeader header;
        m_pimpl->m_blockchain.header_at(index - 1, header);

        header_response.block_headers.push_back(std::move(header));
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
    bool bad_data = header_response.block_headers.empty();

    if (bad_data) throw wrong_data_exception("blockheader response. empty response received!");

    auto r_it = header_response.block_headers.begin();
    if (r_it->block_number == tmp_header.block_number && m_pimpl->sync_headers.empty())
        bad_data = r_it->consensus_sum <= tmp_header.consensus_sum;

    if (bad_data) throw wrong_data_exception("blockheader response. wrong data received!");

    if (!m_pimpl->sync_headers.empty() && // we have something received before
        tmp_header.block_number >= m_pimpl->sync_headers.rbegin()->block_number)
    {
        // load next mot checked header
        m_pimpl->m_blockchain.header_at(m_pimpl->sync_headers.rbegin()->block_number - 1, tmp_header);

        bad_data = bad_data || tmp_header.block_number != r_it->block_number;
    }

    if (bad_data) throw wrong_data_exception("blockheader response. unexpected data received!");

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
        if (r_it->previous_hash == tmp_hash &&
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
               r_it->consensus_sum > tmp_header.consensus_sum)
        {
            m_pimpl->sync_headers.push_back(std::move(*r_it++));
            m_pimpl->m_blockchain.header_at(tmp_header.block_number - 1, tmp_header);
        }

        for (; !lcb_found && r_it != header_response.block_headers.end(); ++r_it)
        {
            if (tmp_header.previous_hash == r_it->previous_hash)
                lcb_found = true;

            if (tmp_header.block_number > 0)
            {
                m_pimpl->sync_headers.push_back(std::move(*r_it));
                m_pimpl->m_blockchain.header_at(tmp_header.block_number - 1, tmp_header);
            }
        }

        if (lcb_found)
        {
            bad_data = check_headers(*m_pimpl->sync_headers.rbegin(), tmp_header);

            bad_data = bad_data || check_headers_vector(m_pimpl->sync_headers);

            if (bad_data) throw wrong_data_exception("blockheader response. header check failed!");

            // verify consensus_const
            vector<pair<uint64_t, uint64_t>> delta_vector;
        
            for (auto it = m_pimpl->sync_headers.begin(); it != m_pimpl->sync_headers.end(); ++it)
                delta_vector.push_back(pair<uint64_t, uint64_t>(it->consensus_delta, it->consensus_const));
        
            uint64_t number = m_pimpl->sync_headers.rbegin()->block_number - 1;
            uint64_t delta_step = number < DELTA_STEP ? number : DELTA_STEP;
        
            for (uint64_t i = 0; i < delta_step; ++i)
            {
                BlockHeader _header;
                m_pimpl->m_blockchain.header_at(number - i, _header);
        
                delta_vector.push_back(pair<uint64_t, uint64_t>(_header.consensus_delta, _header.consensus_const));
            }
        
            for (auto it = delta_vector.begin(); !bad_data && it + delta_step != delta_vector.end(); ++it)
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
        
                    if (step >= DELTA_STEP)
                        bad_data = it->second != (it + 1)->second * 2;
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
        
                    if (step >= DELTA_STEP)
                        bad_data = it->second != (it + 1)->second / 2;
                }
            }
            
            if (bad_data) throw wrong_data_exception("blockheader response. wrong consensus const!");

            //3. request blockchain from found point
            BlockChainRequest blockchain_request;
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
                    response.signed_blocks.size() > m_pimpl->sync_headers.size() -
                                                    m_pimpl->sync_blocks.size();

    if (bad_data) throw wrong_data_exception("blockchain response. too many blocks in response!");

    // find last common block
    uint64_t block_number = (*m_pimpl->sync_headers.rbegin()).block_number;

    if (block_number == 0) throw wrong_data_exception("blockchain response. uzum en qcen!");

    //2. check and add received blockchain to sync_blocks_vector for future process
    size_t length = m_pimpl->sync_blocks.size();

    // put prev_signed_block in correct place
    SignedBlock prev_signed_block;
    if (m_pimpl->sync_blocks.empty())
        m_pimpl->m_blockchain.at(block_number - 1, prev_signed_block);
    else
        prev_signed_block = *m_pimpl->sync_blocks.rbegin();

    for (auto it = response.signed_blocks.begin(); it != response.signed_blocks.end(); ++it)
    {
        // verify block signature
        bool sb_verify = meshpp::verify_signature(meshpp::public_key(it->authority),
                                                  it->block_details.to_string(),
                                                  it->signature);

        if (!sb_verify) throw wrong_data_exception("blockchain response. wrong block signature!");

        // store blocks for future use
        m_pimpl->sync_blocks.push_back(std::move(*it));
    }

    auto block_it = m_pimpl->sync_blocks.begin() + length;
    auto header_it = m_pimpl->sync_headers.rbegin() + length;

    bad_data = header_it->previous_hash != meshpp::hash(prev_signed_block.block_details.to_string());

    ++header_it;
    while (!bad_data && block_it != m_pimpl->sync_blocks.end() && header_it != m_pimpl->sync_headers.rend())
    {
        Block block;
        block_it->block_details.get(block);

        bad_data = *(header_it - 1) != block.header ||
                     header_it->previous_hash != meshpp::hash(block_it->block_details.to_string());
        ++block_it;
        ++header_it;
    }

    if(bad_data) throw wrong_data_exception("blockchain response. blocks are not correspond to headers!");

    // request new chain if needed
    if (m_pimpl->sync_blocks.size() < m_pimpl->sync_headers.size())
    {
        BlockChainRequest blockchain_request;
        blockchain_request.blocks_from = header_it->block_number - 1;
        blockchain_request.blocks_to = m_pimpl->sync_headers.begin()->block_number;

        sk.send(peerid, blockchain_request);
        m_pimpl->update_sync_time();
        m_pimpl->store_request(peerid, blockchain_request);

        return; // will wait new chain
    }

    //3. all needed blocks received, start to check
    std::unordered_map<string, coin> accounts_diff;

    //-----------------------------------------------------//
    auto get_balance = [&](string const& key)
    {
        auto accounts_it = accounts_diff.find(key);
        if (accounts_it != accounts_diff.end())
            return accounts_it->second;
        else
            return coin(m_pimpl->m_state.get_balance(key));
    };

    auto increase_balance = [&](string const& key, coin const& amount)
    {
        auto balance = get_balance(key);

        accounts_diff[key] = balance + amount;
    };

    auto decrease_balance = [&](string const& key, coin const& amount)
    {
        auto balance = get_balance(key);

        if (balance < amount)
            return false;

        accounts_diff[key] = balance - amount;

        return true;
    };
    //-----------------------------------------------------//

    // calculate back to get accounts_diff at LCB point
    block_number = m_pimpl->m_blockchain.length() - 1;
    uint64_t lcb_number = (*m_pimpl->sync_headers.rbegin()).block_number - 1;
    while (block_number > lcb_number)
    {
        SignedBlock signed_block;
        m_pimpl->m_blockchain.at(block_number, signed_block);

        Block block;
        std::move(signed_block.block_details).get(block);
        
        // decrease all reward amounts from balances
        for (auto it = block.rewards.begin(); it != block.rewards.end(); ++it)
            if (!decrease_balance(it->to, it->amount))
                throw std::runtime_error("It's a catastrophe!");

        // calculate back transactions
        coin fee;
        for (auto it = block.signed_transactions.rbegin(); it != block.signed_transactions.rend(); ++it)
        {
            fee += it->transaction_details.fee;

            Transfer transfer;
            std::move(it->transaction_details.action).get(transfer);

            // revert sender balance
            increase_balance(transfer.from, transfer.amount + it->transaction_details.fee);

            // revert receiver balance
            if(!decrease_balance(transfer.to, transfer.amount))
                throw std::runtime_error("It's a catastrophe!");
        }

        // revert authority balance
        if(!decrease_balance(signed_block.authority, fee))
            throw std::runtime_error("It's a catastrophe!");

        --block_number;
    }

    // verify new received blocks
    Block prev_block;
    m_pimpl->m_blockchain.at(lcb_number, prev_signed_block);
    std::move(prev_signed_block.block_details).get(prev_block);

    for (block_it = m_pimpl->sync_blocks.begin(); block_it != m_pimpl->sync_blocks.end(); ++block_it)
    {
        Block block;
        block_it->block_details.get(block);

        // verify consensus_delta
        coin amount = get_balance(block_it->authority);
        uint64_t delta = calc_delta(block_it->authority, amount.to_uint64_t(), prev_block.header);
        
        bad_data = delta != block.header.consensus_delta;
        if (bad_data) break;

        // verify miner balance at mining time
        bad_data = amount < MINE_AMOUNT_THRESHOLD;
        if (bad_data) break;

        // verify block transactions
        coin fee;
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
    }

    if (bad_data) throw wrong_data_exception("blockchain response. wrong data received!");

    //4. apply received chain
    vector<SignedBlock> backup_blocks;
    uint64_t count = m_pimpl->m_blockchain.length() - m_pimpl->sync_headers.rbegin()->block_number;

    revert_blocks(count, m_pimpl, backup_blocks);

    if(!insert_blocks(m_pimpl->sync_blocks, m_pimpl))
        if (!insert_blocks(backup_blocks, m_pimpl))
            throw std::runtime_error("Something wrong happenes. Cant't insert back own chain!");
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
