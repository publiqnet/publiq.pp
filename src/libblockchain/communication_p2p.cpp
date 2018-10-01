#include "communication_p2p.hpp"

#include "coin.hpp"
#include "common.hpp"
#include "node_internals.hpp"

#include <mesh.pp/cryptoutility.hpp>

#include <map>

using namespace BlockchainMessage;

using std::multimap;

///////////////////////////////////////////////////////////////////////////////////
//                            Internal Finctions
bool apply_transaction(SignedTransaction& signed_transaction, 
                       unique_ptr<publiqpp::detail::node_internals>& m_pimpl, 
                       string const key = string())
{
    coin fee;
    Transfer transfer;
    signed_transaction.transaction_details.action.get(transfer);

    if (!key.empty())
    {
        fee = signed_transaction.transaction_details.fee;

        Coin balance = m_pimpl->m_state.get_balance(transfer.from);

        if (coin(balance) < transfer.amount + fee)
            return false;
    }

    m_pimpl->m_state.decrease_balance(transfer.from, transfer.amount + fee);
    m_pimpl->m_state.increase_balance(transfer.to, transfer.amount);

    if (!fee.empty())
        m_pimpl->m_state.increase_balance(key, fee);

    return true;
}

void revert_transaction(Transaction& transaction, 
                        unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                        string const key = string())
{
    coin fee;
    Transfer transfer;
    transaction.action.get(transfer);

    if (!key.empty())
        fee = transaction.fee;

    m_pimpl->m_state.increase_balance(transfer.from, transfer.amount + fee);
    m_pimpl->m_state.decrease_balance(transfer.to, transfer.amount);

    if (!fee.empty())
        m_pimpl->m_state.decrease_balance(key, fee);
}

coin miner_reward(uint64_t block_number)
{
//    coin reward = MINE_INIT_PART * MINE_REWARD_BUDGET;
//
//    if (block_number >= MINE_YEAR_STEP)
//        reward = pow(1 - MINE_INIT_PART, block_number / MINE_YEAR_STEP) * MINE_REWARD_BUDGET;
//
//    reward = reward / MINE_YEAR_STEP;
//
//    return reward;
    B_UNUSED(block_number);

    return coin(1, 0);
}

void grant_rewards(vector<SignedTransaction> const& signed_transactions, vector<Reward>& rewards, string const& authority, uint64_t block_number)
{
    rewards.clear();

    for (auto it = signed_transactions.begin(); it != signed_transactions.end(); ++it)
    {
        //TODO grant real rewards from transactions
    }

    // add own rewards
    Reward own_reward;

    // grant miner reward himself
    own_reward.to = authority;
    own_reward.amount = miner_reward(block_number).to_Coin();
    
    rewards.push_back(own_reward);
}

bool check_headers(BlockHeader const& next_header, BlockHeader const& header)
{
    bool t = next_header.block_number != header.block_number + 1;
    t = t || next_header.c_sum <= header.c_sum;
    t = t || next_header.c_sum != next_header.delta + header.c_sum;
    t = t || (next_header.c_const != header.c_const &&
              next_header.c_const != 2 * header.c_const);

    system_clock::time_point time_point1 = system_clock::from_time_t(header.sign_time.tm);
    system_clock::time_point time_point2 = system_clock::from_time_t(next_header.sign_time.tm);
    chrono::seconds diff_seconds = chrono::duration_cast<chrono::seconds>(time_point2 - time_point1);

    return t || time_point1 > time_point2 || diff_seconds.count() < BLOCK_MINE_DELAY;
};

bool check_rewards(Block const& block, string const& authority)
{
    vector<Reward> rewards;
    grant_rewards(block.signed_transactions, rewards, authority, block.header.block_number);

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

///////////////////////////////////////////////////////////////////////////////////

void mine_block(unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    SignedBlock prev_signed_block;
    uint64_t block_number = m_pimpl->m_blockchain.length() - 1;
    m_pimpl->m_blockchain.at(block_number, prev_signed_block);

    BlockHeader prev_header;
    m_pimpl->m_blockchain.header(prev_header);

    string own_key = m_pimpl->m_pv_key.get_public_key().to_string();
    string prev_hash = meshpp::hash(prev_signed_block.block_details.to_string());
    uint64_t delta = m_pimpl->calc_delta(own_key, m_pimpl->m_balance.whole, prev_hash, prev_header.c_const);

    // fill new block header data
    BlockHeader block_header;
    block_header.block_number = block_number + 1;
    block_header.delta = delta;
    block_header.c_const = prev_header.c_const;
    block_header.c_sum = prev_header.c_sum + delta;
    block_header.prev_hash = prev_hash;
    block_header.sign_time.tm = system_clock::to_time_t(system_clock::now());

    // update consensus_const if needed
    if (delta > DELTA_UP)
    {
        size_t step = 0;
        BlockHeader prev_header_local;
        m_pimpl->m_blockchain.header_at(block_number, prev_header_local);

        while (prev_header_local.delta > DELTA_UP &&
            step < DELTA_STEP && prev_header_local.block_number > 0)
        {
            ++step;
            m_pimpl->m_blockchain.header_at(prev_header_local.block_number - 1, prev_header_local);
        }

        if (step >= DELTA_STEP)
            block_header.c_const = prev_header_local.c_const * 2;
    }
    else
        if (delta < DELTA_DOWN && block_header.c_const > 1)
        {
            size_t step = 0;
            BlockHeader prev_header_local;
            m_pimpl->m_blockchain.header_at(block_number, prev_header_local);

            while (prev_header_local.delta < DELTA_DOWN &&
                step < DELTA_STEP && prev_header_local.block_number > 0)
            {
                ++step;
                m_pimpl->m_blockchain.header_at(prev_header_local.block_number - 1, prev_header_local);
            }

            if (step >= DELTA_STEP)
                block_header.c_const = prev_header_local.c_const / 2;
        }

    Block block;
    block.header = block_header;

    beltpp::on_failure guard([&m_pimpl] 
    { 
        m_pimpl->discard();
        m_pimpl->calc_balance();
    });

    multimap<BlockchainMessage::ctime, std::pair<string, SignedTransaction>> transaction_map;

    // Revert action log and state, revert transaction pool records
    vector<string> pool_keys;
    m_pimpl->m_transaction_pool.get_keys(pool_keys);
    for (auto& key : pool_keys)
    {
        SignedTransaction signed_transaction;
        m_pimpl->m_transaction_pool.at(key, signed_transaction);

        m_pimpl->m_action_log.revert();
        revert_transaction(signed_transaction.transaction_details, m_pimpl);

        if (block_header.sign_time.tm > signed_transaction.transaction_details.expiry.tm)
            m_pimpl->m_transaction_pool.remove(key); // already expired transaction
        else
            transaction_map.insert(std::pair<BlockchainMessage::ctime, std::pair<string, SignedTransaction>>(
                                   signed_transaction.transaction_details.creation,
                                   std::pair<string, SignedTransaction>(key, signed_transaction)));
    }

    // check and copy transactions to block
    size_t tr_count = 0;
    auto it = transaction_map.begin();
    for (; it != transaction_map.end() && tr_count < BLOCK_MAX_TRANSACTIONS; ++it)
    {
        // Check block transactions and calculate new state
        if (apply_transaction(it->second.second, m_pimpl, own_key))
        {
            ++tr_count;
            block.signed_transactions.push_back(std::move(it->second.second));
            m_pimpl->m_transaction_cache[it->second.first] = system_clock::from_time_t(it->first.tm);
        }

        m_pimpl->m_transaction_pool.remove(it->second.first);
    }

    // grant rewards and move to block
    grant_rewards(block.signed_transactions, block.rewards, own_key, block.header.block_number);

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

void process_blockheader_response(BlockHeaderResponse const& header_response,
                                  std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                                  beltpp::isocket& sk,
                                  beltpp::isocket::peer_id const& peerid)
{
    // find needed header from own data
    BlockHeader tmp_header;
    m_pimpl->m_blockchain.header(tmp_header);

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
                wrong_data_exception("blockheader response. header check failed!");

            // verify consensus_const
            vector<pair<uint64_t, uint64_t>> delta_vector;
        
            for (auto it = m_pimpl->sync_headers.begin(); it != m_pimpl->sync_headers.end(); ++it)
                delta_vector.push_back(pair<uint64_t, uint64_t>(it->delta, it->c_const));
        
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
                        throw wrong_data_exception("blockheader response. wrong consensus const!");
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
                        throw wrong_data_exception("blockheader response. wrong consensus const!");
                }
            }

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
        header_request.blocks_to = header_request.blocks_from > HEADER_TR_LENGTH ? header_request.blocks_from - HEADER_TR_LENGTH : 0;

        sk.send(peerid, header_request);
        m_pimpl->update_sync_time();
        m_pimpl->store_request(peerid, header_request);
    }
}

void process_blockchain_request(BlockChainRequest const& blockchain_request,
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

    BlockChainResponse chain_response;
    for (auto i = from; i <= to; ++i)
    {
        SignedBlock signed_block;
        m_pimpl->m_blockchain.at(i, signed_block);

        chain_response.signed_blocks.push_back(std::move(signed_block));
    }

    sk.send(peerid, chain_response);
}

void process_blockchain_response(BlockChainResponse const& response,
                                 std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                                 beltpp::isocket& sk,
                                 beltpp::isocket::peer_id const& peerid)
{
    auto str_peerid = [](string const& peerid) -> string
    {
        if (peerid == "PBQ7JEFjtQNjyzwnThepF2jJtCe7cCpUFEaxGdUnN2W9wPP5Nh92G")
            return "$tigran(0)";
        if (peerid == "PBQ8gyokoWdo9tSLcDQQjxdhYgmmnScUPT6YDCaVVoeSFRz1zkGpv")
            return "$tigran(1)";
        if (peerid == "PBQ5LNw1peEL8ZRDEw6ukndHpaob8A43dsh2beYg9cwocHm5r3tPR")
            return "$tigran(2)";
        if (peerid == "PBQ5pFSs7NKc26b3gpeFN17oGYkn3vFEuf8sA4HhZQsF9MfRrXShC")
            return "$tigran(3)";
        if (peerid == "PBQ5Nd79pnM2X6E8NTPPwMXBrX8XigztwU3L51ALPSVBQH2L8tiZw")
            return "$tigran(4)";
        if (peerid == "PBQ4te6LkpCnsu9DyoRUZpmhMypbMwqrpofUWvRgGanY8c2vYciwz")
            return "$tigran(5)";
        if (peerid == "PBQ76Zv5QceNSLibecnMGEKbKo3dVFV6HRuDSuX59mJewJxHPhLwu")
            return "$armen(0)";
        if (peerid == "PBQ7aYzUMXfRcmho8wDwFk1oFyGopjD6ADWG7JR4DxvfJn392mpe4")
            return "$armen(1)";
        if (peerid == "PBQ8MiwBdYzSj38etLYLES4FSuKJnLPkXAJv4MyrLW7YJNiPbh4z6")
            return "$sona(0)";
        if (peerid == "PBQ8VLQxxbfD8SNp5LWy2y8rEvLsqcLpKsWCdKqhAEgsjpyhNVqkf")
            return "$sona(1)";
        if (peerid == "PBQ8f5Z8SKVrYFES1KLHtCYMx276a5NTgZX6baahzTqkzfnB4Pidk")
            return "$gagik(0)";
        if (peerid == "PBQ87WZycpRYUWcVC9wB3PL5QgYiZRh3Adg8FWAjtTo2GykFj3anC")
            return "$gagik(1)";
        if (peerid == "PBQ7Ta31VaxCB9VfDRvYYosKYpzxXNgVH46UkM9i4FhzNg4JEU3YJ")
            return "$north.publiq.network:12222";   //  node(0)
        if (peerid == "PBQ4vj4CpQ11HTWg7wSFY3cg5gR4qBxgJJi2uSNJGNTmF22qt5Mbg")
            return "$north.publiq.network:13333";   //  state(0)

        return peerid;
    };

    //1. check received blockchain validity

    if (response.signed_blocks.empty())
        throw wrong_data_exception("blockchain response. empty response received!");

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

    auto header_it = m_pimpl->sync_headers.rbegin() + length;

    if (header_it->prev_hash != meshpp::hash(prev_signed_block.block_details.to_string()))
        throw wrong_data_exception("blockchain response. previous hash!");

    ++header_it;
    for (auto block_it = response.signed_blocks.begin(); block_it != response.signed_blocks.end(); ++block_it)
    {
        Block block;
        block_it->block_details.get(block);
        string str = block.to_string();

        // verify block signature
        if (!meshpp::verify_signature(meshpp::public_key(block_it->authority), str, block_it->signature))
            throw wrong_data_exception("blockchain response. block signature!");

        if (header_it != m_pimpl->sync_headers.rend())
        {
            if (*(header_it - 1) != block.header || header_it->prev_hash != meshpp::hash(str))
                throw wrong_data_exception("blockchain response. block header!");

            ++header_it;
        }

        // verify block rewards
        if (check_rewards(block, block_it->authority))
            throw wrong_data_exception("blockchain response. block rewards!");

        // verify block transactions
        system_clock::time_point delta = system_clock::from_time_t(NODES_TIME_SHIFT);
        for (auto tr_it = block.signed_transactions.begin(); tr_it != block.signed_transactions.end(); ++tr_it)
        {
            if (!meshpp::verify_signature(meshpp::public_key(tr_it->authority),
                                          tr_it->transaction_details.to_string(),
                                          tr_it->signature))
                throw wrong_data_exception("blockchain response. transaction signature!");

            Transfer transfer;
            std::move(tr_it->transaction_details.action).get(transfer);

            if (tr_it->authority != transfer.from)
                throw wrong_data_exception("blockchain response. transaction authority!");

            system_clock::time_point creation = system_clock::from_time_t(tr_it->transaction_details.creation.tm);
            system_clock::time_point expiry = system_clock::from_time_t(tr_it->transaction_details.expiry.tm);

            if (chrono::duration_cast<chrono::seconds>(expiry - creation).count() > TRANSACTION_LIFETIME)
                throw wrong_data_exception("blockchain response. too long lifetime for transaction");

            if (system_clock::time_point(creation - delta) > system_clock::from_time_t(block.header.sign_time.tm))
                throw wrong_data_exception("blockchain response. transaction from the future!");

            if (expiry < system_clock::from_time_t(block.header.sign_time.tm))
                throw wrong_data_exception("blockchain response. expired transaction!");
        }

        // store blocks for future use
        m_pimpl->sync_blocks.push_back(std::move(*block_it));
    }

    // request new chain if needed
    if (m_pimpl->sync_blocks.size() < BLOCK_INSERT_LENGTH &&
        m_pimpl->sync_blocks.size() < m_pimpl->sync_headers.size())
    {
        BlockChainRequest blockchain_request;
        blockchain_request.blocks_from = (header_it - 1)->block_number;
        blockchain_request.blocks_to = m_pimpl->sync_headers.begin()->block_number;

        sk.send(peerid, blockchain_request);
        m_pimpl->update_sync_time();
        m_pimpl->store_request(peerid, blockchain_request);

        return; // will wait new chain
    }

    // test log
    m_pimpl->writeln_node("applying collected " + std::to_string(m_pimpl->sync_blocks.size()) + " blocks");

    if(m_pimpl->sync_blocks.size() == 1)
        m_pimpl->writeln_node("block mined by " + str_peerid(m_pimpl->sync_blocks.rbegin()->authority));

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
    m_pimpl->m_transaction_pool.get_keys(pool_keys);
    bool clear_pool = m_pimpl->sync_blocks.size() < m_pimpl->sync_headers.size();

    for (auto& key : pool_keys)
    {
        SignedTransaction signed_transaction;
        m_pimpl->m_transaction_pool.at(key, signed_transaction);

        m_pimpl->m_action_log.revert();
        revert_transaction(signed_transaction.transaction_details, m_pimpl);

        // This will make sync process faster
        // Most probably removed transactions we will get with blocks
        if (clear_pool)
            m_pimpl->m_transaction_pool.remove(key);
    }

    // calculate back to get state at LCB point
    block_number = m_pimpl->m_blockchain.length() - 1;
    uint64_t lcb_number = m_pimpl->sync_headers.rbegin()->block_number - 1;
    while (block_number > lcb_number)
    {
        SignedBlock signed_block;
        m_pimpl->m_blockchain.at(block_number, signed_block);
        m_pimpl->m_blockchain.remove_last_block();
        m_pimpl->m_action_log.revert();

        Block block;
        std::move(signed_block.block_details).get(block);

        // decrease all reward amounts from balances and revert reward
        for (auto it = block.rewards.rbegin(); it != block.rewards.rend(); ++it)
            m_pimpl->m_state.decrease_balance(it->to, it->amount);

        // calculate back transactions
        for (auto it = block.signed_transactions.rbegin(); it != block.signed_transactions.rend(); ++it)
        {
            revert_transaction(it->transaction_details, m_pimpl, signed_block.authority);

            string key = meshpp::hash((*it).to_string());
            m_pimpl->m_transaction_cache.erase(key);

            if (now <= system_clock::from_time_t(it->transaction_details.expiry.tm))
                m_pimpl->m_transaction_pool.insert(*it); // still not expired transaction
        }

        --block_number;
    }

    // verify new received blocks
    Block block;
    BlockHeader prev_header;
    m_pimpl->m_blockchain.header_at(lcb_number, prev_header);
    uint64_t c_const = prev_header.c_const;

    for (auto block_it = m_pimpl->sync_blocks.begin(); block_it != m_pimpl->sync_blocks.end(); ++block_it)
    {
        block_it->block_details.get(block);

        // verify consensus_delta
        Coin amount = m_pimpl->m_state.get_balance(block_it->authority);
        uint64_t delta = m_pimpl->calc_delta(block_it->authority, amount.whole, block.header.prev_hash, c_const);
        
        if (delta != block.header.delta)
            throw wrong_data_exception("blockchain response. consensus delta!");

        // verify miner balance at mining time
        if (coin(amount) < MINE_AMOUNT_THRESHOLD)
            throw wrong_data_exception("blockchain response. miner balance!");

        // verify block transactions
        for (auto tr_it = block.signed_transactions.begin(); tr_it != block.signed_transactions.end(); ++tr_it)
        {
            string key = meshpp::hash((*tr_it).to_string());

            m_pimpl->m_transaction_pool.remove(key);

            if (m_pimpl->m_transaction_cache.find(key) != m_pimpl->m_transaction_cache.end())
                throw wrong_data_exception("blockchain response. transaction double use!");

            m_pimpl->m_transaction_cache[key] = system_clock::from_time_t(tr_it->transaction_details.creation.tm);

            if(!apply_transaction(*tr_it, m_pimpl, block_it->authority))
                throw wrong_data_exception("blockchain response. sender balance!");
        }

        // increase all reward amounts to balances
        for (auto reward_it = block.rewards.begin(); reward_it != block.rewards.end(); ++reward_it)
            m_pimpl->m_state.increase_balance(reward_it->to, reward_it->amount);

        // Insert to blockchain
        m_pimpl->m_blockchain.insert(*block_it);
        m_pimpl->m_action_log.log_block(*block_it);

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
            m_pimpl->m_transaction_pool.remove(key); // not enough balance
    }

    m_pimpl->save(guard);
    m_pimpl->calc_sync_info(block);

    // request new chain if the process was stopped
    // by BLOCK_INSERT_LENGTH restriction
    length = m_pimpl->sync_blocks.size();
    if (length < m_pimpl->sync_headers.size())
    {
        // clear already inserted blocks and headers
        m_pimpl->sync_blocks.clear();
        for (size_t i = 0; i < length; ++i)
            m_pimpl->sync_headers.pop_back();

        BlockChainRequest blockchain_request;
        blockchain_request.blocks_from = m_pimpl->sync_headers.rbegin()->block_number;
        blockchain_request.blocks_to = m_pimpl->sync_headers.begin()->block_number;

        sk.send(peerid, blockchain_request);
        m_pimpl->update_sync_time();
        m_pimpl->store_request(peerid, blockchain_request);
    }
    else
        m_pimpl->clear_sync_state(m_pimpl->sync_peerid);
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
