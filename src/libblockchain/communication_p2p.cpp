#include "communication_p2p.hpp"
#include "communication_rpc.hpp"

#include "coin.hpp"
#include "common.hpp"

#include <mesh.pp/cryptoutility.hpp>

#include <map>

using namespace BlockchainMessage;

using std::multimap;

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

        if (!key.empty())
        {
            Coin balance = m_pimpl->m_state.get_balance(contract.owner);

            if (coin(balance) < fee)
                return false;

            m_pimpl->m_state.decrease_balance(contract.owner, fee);
        }

        m_pimpl->m_state.insert_contract(contract);
    }
    else
        throw wrong_data_exception("unknown transaction action type!");

    if (!fee.empty())
        m_pimpl->m_state.increase_balance(key, fee);

    return true;
}

void revert_transaction(Transaction& transaction, 
                        unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                        bool const increase,
                        string const& key = string())
{
    coin fee;

    if (!key.empty())
    {
        fee = transaction.fee;

        if (!increase)
            m_pimpl->m_state.decrease_balance(key, fee);
    }

    if (transaction.action.type() == Transfer::rtt)
    {
        Transfer transfer;
        transaction.action.get(transfer);

        if (increase)
            m_pimpl->m_state.increase_balance(transfer.from, transfer.amount + fee);
        else
            m_pimpl->m_state.decrease_balance(transfer.to, transfer.amount);
    }
    else if (transaction.action.type() == Contract::rtt)
    {
        Contract contract;
        transaction.action.get(contract);

        if (increase)
            m_pimpl->m_state.increase_balance(contract.owner, fee);
        else
            m_pimpl->m_state.remove_contract(contract);
    }
    else
        throw wrong_data_exception("unknown transaction action type!");
}

void grant_rewards(vector<SignedTransaction> const& signed_transactions, 
                   vector<Reward>& rewards, 
                   string const& authority, 
                   uint64_t block_number,
                   std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    rewards.clear();

    for (auto it = signed_transactions.begin(); it != signed_transactions.end(); ++it)
    {
        //TODO grant real rewards from transactions
    }

    size_t year_index = block_number / 50000;

    if (year_index < 60)
    {
        Reward reward;

        // grant channel rewards
        std::vector<Contract> channel_contracts;
        m_pimpl->m_state.get_contracts(channel_contracts, publiqpp::detail::node_type_to_int(publiqpp::node_type::channel));

        for (auto const& item : channel_contracts)
        {
            reward.to = item.owner;
            reward.amount = coin(60, 0).to_Coin();

            rewards.push_back(reward);
        }

        // grant storage rewards
        std::vector<Contract> storage_contracts;
        m_pimpl->m_state.get_contracts(storage_contracts, publiqpp::detail::node_type_to_int(publiqpp::node_type::storage));

        for (auto const& item : storage_contracts)
        {
            reward.to = item.owner;
            reward.amount = coin(30, 0).to_Coin();

            rewards.push_back(reward);
        }

        // grant miner reward himself
        reward.to = authority;
        reward.amount = coin(100, 0).to_Coin();// BLOCK_REWARD_ARRAY[year_index].to_Coin();

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
{
    vector<Contract> storages;
    m_pimpl->m_state.get_contracts(storages, publiqpp::detail::node_type_to_int(publiqpp::node_type::storage));

    if (storages.empty()) return;

    SignedBlock signed_block;
    uint64_t block_number = m_pimpl->m_blockchain.length() - 1;
    m_pimpl->m_blockchain.at(block_number, signed_block);

    StatInfo storage_info;
    storage_info.block_hash = meshpp::hash(signed_block.block_details.to_string());

    for (auto& contract : storages)
    {
        StatItem stat_item;
        stat_item.node_name = contract.owner;
        stat_item.content_hash = meshpp::hash("storage");
        stat_item.pass_count = 1;
        stat_item.fail_count = 0;

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

    process_broadcast(std::move(broadcast),
                      m_pimpl->m_ptr_p2p_socket->name(),
                      m_pimpl->m_ptr_p2p_socket->name(),
                      true,
                      nullptr,
                      m_pimpl->m_p2p_peers,
                      m_pimpl->m_ptr_p2p_socket.get());
}

void broadcast_channel_info(std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    vector<Contract> channels;
    m_pimpl->m_state.get_contracts(channels, publiqpp::detail::node_type_to_int(publiqpp::node_type::storage));

    if (channels.empty()) return;

    SignedBlock signed_block;
    uint64_t block_number = m_pimpl->m_blockchain.length() - 1;
    m_pimpl->m_blockchain.at(block_number, signed_block);

    StatInfo channel_info;
    channel_info.block_hash = meshpp::hash(signed_block.block_details.to_string());

    for (auto& contract : channels)
    {
        StatItem stat_item;
        stat_item.node_name = contract.owner;
        stat_item.content_hash = meshpp::hash("channel");
        stat_item.pass_count = 1;
        stat_item.fail_count = 0;

        channel_info.items.push_back(stat_item);
    }

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

    process_broadcast(std::move(broadcast),
                      m_pimpl->m_ptr_p2p_socket->name(),
                      m_pimpl->m_ptr_p2p_socket->name(),
                      true,
                      nullptr,
                      m_pimpl->m_p2p_peers,
                      m_pimpl->m_ptr_p2p_socket.get());
}

///////////////////////////////////////////////////////////////////////////////////

void mine_block(unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    SignedBlock prev_signed_block;
    uint64_t block_number = m_pimpl->m_blockchain.length() - 1;
    m_pimpl->m_blockchain.at(block_number, prev_signed_block);

    BlockHeader prev_header;
    m_pimpl->m_blockchain.header(prev_header);

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

    multimap<BlockchainMessage::ctime, std::pair<string, SignedTransaction>> transaction_map;

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
        revert_transaction(signed_transaction.transaction_details, m_pimpl, true);

        if (block_header.time_signed.tm > signed_transaction.transaction_details.expiry.tm)
            m_pimpl->m_transaction_pool.remove(key); // already expired transaction
        else
            transaction_map.insert(std::pair<BlockchainMessage::ctime, std::pair<string, SignedTransaction>>(
                                   signed_transaction.transaction_details.creation,
                                   std::pair<string, SignedTransaction>(key, signed_transaction)));
    }

    // complete revert started above
    for (auto& signed_transaction : pool_transactions)
        revert_transaction(signed_transaction.transaction_details, m_pimpl, false);

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
    m_pimpl->writeln_node("applying collected " + std::to_string(m_pimpl->sync_blocks.size()) + " blocks");

    if(m_pimpl->sync_blocks.size() == 1)
        m_pimpl->writeln_node("block mined by " +
                              publiqpp::detail::peer_short_names(m_pimpl->sync_blocks.rbegin()->authority));

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
        revert_transaction(signed_transaction.transaction_details, m_pimpl, true);

        // This will make sync process faster
        // Most probably removed transactions we will get with blocks
        if (clear_pool)
            m_pimpl->m_transaction_pool.remove(key);
    }

    // complete revert started above
    for(auto& signed_transaction : pool_transactions)
        revert_transaction(signed_transaction.transaction_details, m_pimpl, false);

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
            revert_transaction(it->transaction_details, m_pimpl, true, signed_block.authority);
            revert_transaction(it->transaction_details, m_pimpl, false, signed_block.authority);

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
            m_pimpl->m_transaction_pool.remove(key); // not enough balance
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

        if (m_pimpl->m_node_type == publiqpp::node_type::channel)
            broadcast_storage_info(m_pimpl);

        if (m_pimpl->m_node_type == publiqpp::node_type::storage)
            broadcast_channel_info(m_pimpl);
    }
}

void broadcast_node_type(std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    publiqpp::node_type my_type;

    if (m_pimpl->m_node_type == publiqpp::node_type::channel &&
        m_pimpl->m_state.get_contract_type(m_pimpl->m_pb_key.to_string()) != 
        publiqpp::detail::node_type_to_int(publiqpp::node_type::channel))
    {
        my_type = publiqpp::node_type::channel;
    }
    else 
    if (m_pimpl->m_node_type == publiqpp::node_type::storage &&
        m_pimpl->m_state.get_contract_type(m_pimpl->m_pb_key.to_string()) != 
        publiqpp::detail::node_type_to_int(publiqpp::node_type::storage))
    {
        my_type = publiqpp::node_type::storage;
    }
    else
        return;

    Contract contract;
    contract.owner = m_pimpl->m_pb_key.to_string();
    contract.type = publiqpp::detail::node_type_to_int(my_type);

    Transaction transaction;
    transaction.action = contract;
    transaction.creation.tm = system_clock::to_time_t(system_clock::now());
    transaction.expiry.tm = system_clock::to_time_t(system_clock::now() + chrono::hours(24));

    SignedTransaction signed_transaction;
    signed_transaction.transaction_details = transaction;
    signed_transaction.authority = m_pimpl->m_pb_key.to_string();
    signed_transaction.signature = m_pimpl->m_pv_key.sign(transaction.to_string()).base58;

    Broadcast broadcast;
    broadcast.echoes = 2;
    broadcast.package = signed_transaction;

    process_broadcast(std::move(broadcast),
                      m_pimpl->m_ptr_p2p_socket->name(),
                      m_pimpl->m_ptr_p2p_socket->name(),
                      true, // broadcast to all peers
                      //m_pimpl->plogger_node,
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
    string tr_hash = meshpp::hash(contract.to_string());

    if (m_pimpl->m_transaction_pool.contains(tr_hash) ||
        m_pimpl->m_transaction_cache.find(tr_hash) != m_pimpl->m_transaction_cache.end())
        return false;

    beltpp::on_failure guard([&m_pimpl] { m_pimpl->discard(); });

    // Add to the pool
    m_pimpl->m_transaction_pool.insert(signed_transaction);

    // Add to action log
    m_pimpl->m_action_log.log_transaction(signed_transaction);

    m_pimpl->save(guard);

    return true;
}

bool process_stat_info(BlockchainMessage::SignedTransaction const& signed_transaction,
                       std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    //TODO check validity

    if (m_pimpl->m_node_type != publiqpp::node_type::miner)
        return true;

    string key = meshpp::hash(signed_transaction.to_string());

    if (m_pimpl->m_stat_cache.find(key) != m_pimpl->m_stat_cache.end())
        return false;

    m_pimpl->m_stat_cache[key] = signed_transaction;

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
