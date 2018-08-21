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

uint64_t calc_delta(string const& key, uint64_t const& amount, string const& prev_hash, uint64_t const& cons_const)
{
    string key_hash = meshpp::hash(key);

    uint64_t dist = meshpp::distance(key_hash, prev_hash);
    uint64_t delta = amount / (dist * cons_const);

    if (delta > DELTA_MAX)
        delta = DELTA_MAX;

    return delta;
}

void apply_tranaction(Transaction& transaction, unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    Transfer transfer;
    transaction.action.get(transfer);

    m_pimpl->m_state.decrease_balance(transfer.from, transfer.amount + transaction.fee);
    m_pimpl->m_state.increase_balance(transfer.to, transfer.amount);

    // log transfer
    m_pimpl->m_action_log.log(std::move(transfer));
}

void revert_transaction(Transaction& transaction, unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    Transfer transfer;
    transaction.action.get(transfer);

    m_pimpl->m_state.increase_balance(transfer.from, transfer.amount + transaction.fee);
    m_pimpl->m_state.decrease_balance(transfer.to, transfer.amount);

    // revert transfer
    m_pimpl->m_action_log.revert();
}

void pool_to_state_log(unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    vector<string> pool_keys;
    m_pimpl->m_transaction_pool.get_keys(pool_keys);

    auto now = system_clock::now();

    for (auto& key : pool_keys)
    {
        SignedTransaction signed_transaction;
        m_pimpl->m_transaction_pool.at(key, signed_transaction);

        if (now > system_clock::from_time_t(signed_transaction.transaction_details.expiry.tm))
            m_pimpl->m_transaction_pool.remove(key);
        else
            apply_tranaction(signed_transaction.transaction_details, m_pimpl);
    }
}

void insert_block(SignedBlock& signed_block,
                  unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    // Revert action log and state, revert transaction pool records
    vector<string> pool_keys;
    m_pimpl->m_transaction_pool.get_keys(pool_keys);
    for (auto& key : pool_keys)
    {
        SignedTransaction signed_transaction;
        m_pimpl->m_transaction_pool.at(key, signed_transaction);

        revert_transaction(signed_transaction.transaction_details, m_pimpl);
    }

    Block block;
    signed_block.block_details.get(block);

    // Check block transactions and calculate new state
    for (auto& signed_transaction : block.signed_transactions)
    {
        apply_tranaction(signed_transaction.transaction_details, m_pimpl);

        // remove transaction from pool if exists
        m_pimpl->m_transaction_pool.remove(meshpp::hash(signed_transaction.to_string()));
    }

    // apply rewards to state and action_log
    for (auto& reward : block.rewards)
    {
        m_pimpl->m_state.increase_balance(reward.to, reward.amount);
        m_pimpl->m_action_log.log(std::move(reward));
    }

    // insert to blockchain
    m_pimpl->m_blockchain.insert(std::move(signed_block));

    // apply back pool content to state and action_log
    pool_to_state_log(m_pimpl);
}

void grant_rewards(vector<SignedTransaction> const& signed_transactions, vector<Reward>& rewards, string const& authority)
{
    rewards.clear();

    coin fee;
    for (auto it = signed_transactions.begin(); it != signed_transactions.end(); ++it)
        fee += it->transaction_details.fee;

    //TODO grant real rewards from transactions

    // add own rewards
    if (!signed_transactions.empty() || !rewards.empty())
    {
        Reward own_reward;

        if (!fee.empty())
        {
            // grant fee reward himself
            own_reward.amount = fee.to_Coin();
            own_reward.to = authority;
            rewards.push_back(own_reward);
        }

        // grant miner reward himself
        own_reward.amount = MINER_REWARD.to_Coin();
        own_reward.to = authority;
        rewards.push_back(own_reward);
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

bool check_rewards(Block const& block, string const& authority)
{
    vector<Reward> rewards;
    grant_rewards(block.signed_transactions, rewards, authority);

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

    Reward reward;
    reward.amount = coin(100, 0).to_Coin();

    meshpp::random_seed node_rs("NODE");
    meshpp::private_key node_pv = node_rs.get_private_key(0);
    reward.to = node_pv.get_public_key().to_string();
    block.rewards.push_back(std::move(reward));

    meshpp::random_seed armen_rs("Armen");
    meshpp::private_key armen_pv = armen_rs.get_private_key(0);
    reward.to = armen_pv.get_public_key().to_string();
    block.rewards.push_back(std::move(reward));

    meshpp::random_seed tigran_rs("Tigran");
    meshpp::private_key tigran_pv = tigran_rs.get_private_key(0);
    reward.to = tigran_pv.get_public_key().to_string();
    block.rewards.push_back(std::move(reward));

    meshpp::random_seed gagik_rs("Gagik");
    meshpp::private_key gagik_pv = gagik_rs.get_private_key(0);
    reward.to = gagik_pv.get_public_key().to_string();
    block.rewards.push_back(std::move(reward));

    meshpp::random_seed sona_rs("Sona");
    meshpp::private_key sona_pv = sona_rs.get_private_key(0);
    reward.to = sona_pv.get_public_key().to_string();
    block.rewards.push_back(std::move(reward));

    meshpp::random_seed rs("GENESIS");
    meshpp::private_key pv = rs.get_private_key(0);
    meshpp::signature sgn = pv.sign(block.to_string());

    SignedBlock signed_block;
    signed_block.signature = sgn.base58;
    signed_block.authority = node_pv.get_public_key().to_string();
    signed_block.block_details = std::move(block);

    beltpp::on_failure guard([&m_pimpl] { m_pimpl->discard(); });

    insert_block(signed_block, m_pimpl);

    m_pimpl->save(guard);
}

void mine_block(unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    SignedBlock prev_signed_block;
    uint64_t block_number = m_pimpl->m_blockchain.length();
    m_pimpl->m_blockchain.at(block_number - 1, prev_signed_block);

    BlockHeader prev_header;
    m_pimpl->m_blockchain.header(prev_header);

    string own_key = m_pimpl->private_key.get_public_key().to_string();
    string prev_hash = meshpp::hash(prev_signed_block.block_details.to_string());
    coin amount = m_pimpl->m_state.get_balance(m_pimpl->private_key.get_public_key().to_string());
    uint64_t delta = calc_delta(own_key, amount.to_uint64_t(), prev_hash, prev_header.consensus_const);

    // fill new block header data
    BlockHeader block_header;
    block_header.block_number = block_number;
    block_header.consensus_delta = delta;
    block_header.consensus_const = prev_header.consensus_const;
    block_header.consensus_sum = prev_header.consensus_sum + delta;
    block_header.previous_hash = prev_hash;
    block_header.sign_time.tm = system_clock::to_time_t(system_clock::now());

    // update consensus_const if needed
    if (delta > DELTA_UP)
    {
        size_t step = 0;
        BlockHeader prev_header_local;
        m_pimpl->m_blockchain.header_at(block_number, prev_header_local);

        while (prev_header_local.consensus_delta > DELTA_UP &&
            step < DELTA_STEP && prev_header_local.block_number > 0)
        {
            ++step;
            m_pimpl->m_blockchain.header_at(prev_header_local.block_number - 1, prev_header_local);
        }

        if (step >= DELTA_STEP)
            block_header.consensus_const = prev_header_local.consensus_const * 2;
    }
    else
        if (delta < DELTA_DOWN && block_header.consensus_const > 1)
        {
            size_t step = 0;
            BlockHeader prev_header_local;
            m_pimpl->m_blockchain.header_at(block_number, prev_header_local);

            while (prev_header_local.consensus_delta < DELTA_DOWN &&
                step < DELTA_STEP && prev_header_local.block_number > 0)
            {
                ++step;
                m_pimpl->m_blockchain.header_at(prev_header_local.block_number - 1, prev_header_local);
            }

            if (step >= DELTA_STEP)
                block_header.consensus_const = prev_header_local.consensus_const / 2;
        }

    Block block;
    block.header = block_header;

    // copy transactions from pool to block
    vector<string> pool_keys;
    m_pimpl->m_transaction_pool.get_keys(pool_keys);

    multimap<BlockchainMessage::ctime, SignedTransaction> transaction_map;

    coin fee;
    for (auto& key : pool_keys)
    {
        SignedTransaction signed_transaction;
        m_pimpl->m_transaction_pool.at(key, signed_transaction);

        if (block_header.sign_time.tm > signed_transaction.transaction_details.expiry.tm)
        {
            // already expired transaction
            m_pimpl->m_transaction_pool.remove(key);
        }
        else
        {
            fee += signed_transaction.transaction_details.fee;

            transaction_map.insert(std::pair<BlockchainMessage::ctime, SignedTransaction>(signed_transaction.transaction_details.creation,
                signed_transaction));
        }
    }

    for (auto it = transaction_map.begin(); it != transaction_map.end(); ++it)
        block.signed_transactions.push_back(std::move(it->second));

    // grant rewards and move to block
    grant_rewards(block.signed_transactions, block.rewards, own_key);

    // sign block and insert to blockchain
    meshpp::signature sgn = m_pimpl->private_key.sign(block.to_string());

    SignedBlock signed_block;
    signed_block.signature = sgn.base58;
    signed_block.authority = own_key;
    signed_block.block_details = std::move(block);

    beltpp::on_failure guard([&m_pimpl] { m_pimpl->discard(); });

    insert_block(signed_block, m_pimpl);

    m_pimpl->save(guard);
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
    if (header_response.block_headers.empty())
        throw wrong_data_exception("blockheader response. empty response received!");

    auto r_it = header_response.block_headers.begin();
    if (r_it->block_number == tmp_header.block_number && m_pimpl->sync_headers.empty() &&
        r_it->consensus_sum <= tmp_header.consensus_sum)
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
            if (check_headers(*m_pimpl->sync_headers.rbegin(), tmp_header) ||
                check_headers_vector(m_pimpl->sync_headers))
                wrong_data_exception("blockheader response. header check failed!");

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

void process_blockchain_response(beltpp::packet& package,
                                 std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                                 beltpp::isocket& sk,
                                 beltpp::isocket::peer_id const& peerid)
{
    beltpp::on_failure guard([&m_pimpl] { m_pimpl->discard(); });

    //1. check received blockchain validity
    BlockChainResponse response;
    std::move(package).get(response);

    if (response.signed_blocks.empty() ||
        response.signed_blocks.size() > m_pimpl->sync_headers.size() - m_pimpl->sync_blocks.size())
        throw wrong_data_exception("blockchain response. wrong content in response!");

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
        if (!meshpp::verify_signature(meshpp::public_key(it->authority),
                                                         it->block_details.to_string(),
                                                         it->signature))
            throw wrong_data_exception("blockchain response. block signature!");

        // store blocks for future use
        m_pimpl->sync_blocks.push_back(std::move(*it));
    }

    auto block_it = m_pimpl->sync_blocks.begin() + length;
    auto header_it = m_pimpl->sync_headers.rbegin() + length;

    if (header_it->previous_hash != meshpp::hash(prev_signed_block.block_details.to_string()))
        throw wrong_data_exception("blockchain response. previous hash!");

    ++header_it;
    while (block_it != m_pimpl->sync_blocks.end() && header_it != m_pimpl->sync_headers.rend())
    {
        Block block;
        block_it->block_details.get(block);

        if (*(header_it - 1) != block.header ||
            header_it->previous_hash != meshpp::hash(block_it->block_details.to_string()))
            throw wrong_data_exception("blockchain response. block header!");

        // verify block rewards
        if (check_rewards(block, block_it->authority))
            throw wrong_data_exception("blockchain response. block rewards!");

        // verify block transactions
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

            if (creation > system_clock::from_time_t(block.header.sign_time.tm))
                throw wrong_data_exception("blockchain response. transaction from the future!");

            if (expiry < system_clock::from_time_t(block.header.sign_time.tm))
                throw wrong_data_exception("blockchain response. expired transaction!");
        }

        ++block_it;
        ++header_it;
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

    //3. all needed blocks received, start to check

    vector<string> pool_keys;
    m_pimpl->m_transaction_pool.get_keys(pool_keys);

    for (auto& key : pool_keys)
    {
        SignedTransaction signed_transaction;
        m_pimpl->m_transaction_pool.at(key, signed_transaction);

        revert_transaction(signed_transaction.transaction_details, m_pimpl);
    }

    // calculate back to get state at LCB point
    block_number = m_pimpl->m_blockchain.length() - 1;
    uint64_t lcb_number = m_pimpl->sync_headers.rbegin()->block_number - 1;
    while (block_number > lcb_number)
    {
        SignedBlock signed_block;
        m_pimpl->m_blockchain.at(block_number, signed_block);
        m_pimpl->m_blockchain.remove_last_block();

        Block block;
        std::move(signed_block.block_details).get(block);
        
        // decrease all reward amounts from balances and revert reward
        for (auto it = block.rewards.rbegin(); it != block.rewards.rend(); ++it)
        {
            m_pimpl->m_action_log.revert();
            m_pimpl->m_state.decrease_balance(it->to, it->amount);
        }

        // calculate back transactions
        for (auto it = block.signed_transactions.rbegin(); it != block.signed_transactions.rend(); ++it)
        {
            m_pimpl->m_transaction_pool.insert(*it);

            revert_transaction(it->transaction_details, m_pimpl);
        }

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
        coin amount = m_pimpl->m_state.get_balance(block_it->authority);
        string prev_hash = meshpp::hash(prev_block.to_string());
        uint64_t delta = calc_delta(block_it->authority, amount.to_uint64_t(), prev_hash, prev_block.header.consensus_const);
        
        if (delta != block.header.consensus_delta)
            throw wrong_data_exception("blockchain response. consensus delta!");

        // verify miner balance at mining time
        if (amount < MINE_AMOUNT_THRESHOLD)
            throw wrong_data_exception("blockchain response. miner balance!");

        // verify block transactions
        for (auto tr_it = block.signed_transactions.begin(); tr_it != block.signed_transactions.end(); ++tr_it)
        {
            m_pimpl->m_transaction_pool.remove((*tr_it).to_string());

            apply_tranaction(tr_it->transaction_details, m_pimpl);
        }

        // increase all reward amounts to balances
        for (auto reward_it = block.rewards.begin(); reward_it != block.rewards.end(); ++reward_it)
        {
            m_pimpl->m_state.increase_balance(reward_it->to, reward_it->amount);
            m_pimpl->m_action_log.log(std::move(*reward_it));
        }

        prev_block = std::move(block);

        // Insert to blockchain
        m_pimpl->m_blockchain.insert(std::move(*block_it));
    }

    // apply back rest of the pool
    pool_to_state_log(m_pimpl);

    m_pimpl->save(guard);

    // request new chain if the process was stopped
    // by BLOCK_INSERT_LENGTH restriction
    length = m_pimpl->sync_blocks.size();
    if (length < m_pimpl->sync_headers.size())
    {
        // clear already inserted blocks and headers
        m_pimpl->sync_blocks.clear();
        for (auto i = 0; i < length; ++i)
            m_pimpl->sync_headers.pop_back();

        BlockChainRequest blockchain_request;
        blockchain_request.blocks_from = m_pimpl->sync_headers.rbegin()->block_number;
        blockchain_request.blocks_to = m_pimpl->sync_headers.begin()->block_number;

        sk.send(peerid, blockchain_request);
        m_pimpl->update_sync_time();
        m_pimpl->store_request(peerid, blockchain_request);
    }
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
