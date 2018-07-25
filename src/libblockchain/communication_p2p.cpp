#include "communication_p2p.hpp"

#include "node_internals.hpp"

#include <mesh.pp/cryptoutility.hpp>

//#include <stack>
//#include <vector>
//#include <chrono>

//using std::vector;
//using std::stack;
//using std::string;
//namespace chrono = std::chrono;
//using system_clock = chrono::system_clock;

using namespace BlockchainMessage;

///////////////////////////////////////////////////////////////////////////////////
bool insert_blocks(std::vector<SignedBlock>& signed_block_vector,
                   std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    std::unordered_set<string> used_key_set;
    std::unordered_set<string> erase_tpool_set;
    std::unordered_map<string, uint64_t> tmp_state;
    std::vector<LoggedTransaction> logged_transactions;
    std::vector<std::pair<std::string, uint64_t>> amounts;

    auto it = signed_block_vector.begin();
    for (; it != signed_block_vector.end(); ++it)
    {
        SignedBlock signed_block = std::move(*it);

        Block block;
        signed_block.block_details.get(block);

        // Check block transactions and calculate new state
        for (auto &signed_transaction : block.block_transactions)
        {
            // Verify signed_transaction signature
            vector<char> buffer = SignedTransaction::saver(&signed_transaction.transaction_details);
            meshpp::signature sg(meshpp::public_key(signed_transaction.authority), buffer, signed_transaction.signature);
            
            sg.check();
            
            beltpp::packet package_transaction = std::move(signed_transaction.transaction_details.action);
            vector<char> packet_vec = package_transaction.save();
            string transfer_hash = meshpp::hash(packet_vec.begin(), packet_vec.end());

            string key;
            uint64_t amount;

            Transaction transaction;
            std::move(package_transaction).get(transaction);

            beltpp::packet package_transfer = std::move(transaction.action);

            Transfer transfer;
            std::move(package_transfer).get(transfer);

            if (signed_transaction.authority != transfer.from)
                return false;

            // correct "from" key balance
            key = transfer.from;
            tmp_state[key] = m_pimpl->m_state.get_balance(key);

            if (used_key_set.find(key) == used_key_set.end())
            {
                used_key_set.insert(key);

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
                tmp_state[key] = amount - transfer.amount - transaction.fee;
            else
                return false;

            // correct to_key balance
            key = transfer.to;
            tmp_state[key] = m_pimpl->m_state.get_balance(key);

            if (used_key_set.find(key) == used_key_set.end())
            {
                used_key_set.insert(key);

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
    }

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
        beltpp::packet packet;
        m_pimpl->m_transaction_pool.at(it, packet);

        Transaction transaction;
        packet.get(transaction);

        if (now > system_clock::from_time_t(transaction.expiry.tm))
        {
            LoggedTransaction action_info;
            action_info.applied_reverted = true;
            action_info.index = 0;
            action_info.action = std::move(packet);

            m_pimpl->m_action_log.insert(action_info);
        }
    }

    return true;
}

void revert_blocks(size_t count,
                   std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    for (size_t i = 0; i < count; ++i)
    {
        uint64_t number = m_pimpl->m_blockchain.length();

        SignedBlock signed_block;
        m_pimpl->m_blockchain.at(number, signed_block);

        Block block;
        std::move(signed_block.block_details).get(block);

        auto it = block.block_transactions.rbegin();

        // Add block transactions to the pool
        for (; it != block.block_transactions.rend(); ++it)
            m_pimpl->m_transaction_pool.insert((*it).transaction_details.action);

        // Remove last block from blockchain
        m_pimpl->m_blockchain.remove_last_block();
    }

    // State shoul be correct :)

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

void process_consensus_request(beltpp::packet& package,
                               std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                               beltpp::isocket& sk,
                               beltpp::isocket::peer_id const& peerid)
{
    ConsensusRequest consensus_request;
    std::move(package).get(consensus_request);

    BlockHeader tmp_header;

    if (m_pimpl->m_blockchain.tmp_header(tmp_header))
    {
        if (tmp_header.block_number < consensus_request.block_number ||
            (tmp_header.block_number == consensus_request.block_number &&
                tmp_header.consensus_delta < consensus_request.consensus_delta))
        {
            // someone have better block
            m_pimpl->m_blockchain.step_disable();
        }
        else
        {
            // I have better block
            ConsensusResponse consensus_response;
            consensus_response.block_number = tmp_header.block_number;
            consensus_response.consensus_delta = tmp_header.consensus_delta;

            sk.send(peerid, consensus_response);
        }
    }
}

void process_consensus_response(beltpp::packet& package,
                                std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    ConsensusResponse consensus_response;
    std::move(package).get(consensus_response);

    BlockHeader tmp_header;

    if (m_pimpl->m_blockchain.tmp_header(tmp_header))
    {
        if (tmp_header.block_number < consensus_response.block_number ||
            (tmp_header.block_number == consensus_response.block_number &&
                tmp_header.consensus_delta < consensus_response.consensus_delta))
        {
            // some peer have better block
            m_pimpl->m_blockchain.step_disable();
        }
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
    to = to < from - 10 ? from - 10 : to;

    BlockHeaderResponse header_response;
    for (auto index = from; index >= to; --to)
    {
        SignedBlock signed_block;
        m_pimpl->m_blockchain.at(index, signed_block);

        Block block;
        std::move(signed_block.block_details).get(block);

        header_response.block_headers.push_back(std::move(block.block_header));
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

    if (!m_pimpl->header_vector.empty() && // we have something received before
        tmp_header.block_number >= m_pimpl->header_vector.rbegin()->block_number)
    {
        // load next mot checked header
        m_pimpl->m_blockchain.header_at(m_pimpl->header_vector.rbegin()->block_number - 1, tmp_header);
    }

    BlockHeaderResponse header_response;
    std::move(package).get(header_response);

    // validate received headers
    auto it = header_response.block_headers.begin();
    bool bad_data = header_response.block_headers.empty();
    bad_data = bad_data ||
        (!m_pimpl->header_vector.empty() &&
            tmp_header.block_number != (*it).block_number);

    for (++it; !bad_data && it != header_response.block_headers.end(); ++it)
    {
        bad_data = bad_data || (*(it - 1)).block_number != (*it).block_number + 1;
        bad_data = bad_data || (*(it - 1)).consensus_sum <= (*it).consensus_sum;
        bad_data = bad_data || (*(it - 1)).consensus_sum != (*(it - 1)).consensus_delta + (*it).consensus_sum;
        bad_data = bad_data || (
            (*(it - 1)).consensus_const != (*it).consensus_const &&
            (*(it - 1)).consensus_const != 2 * (*it).consensus_const
            );
    }

    if (bad_data)
    {
        sk.send(peerid, Drop());
        m_pimpl->remove_peer(peerid);
        m_pimpl->clear_state();
        return;
    }

    // find last common header
    bool found = false;
    it = header_response.block_headers.begin();
    while (!found && it != header_response.block_headers.end())
    {
        if (tmp_header.block_number < (*it).block_number)
        {
            // store for possible use
            m_pimpl->header_vector.push_back(std::move(*it));
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
            if (tmp_header == (*it))
            {
                lcb_found = true;
                continue;
            }
            else if (tmp_header.consensus_sum < (*it).consensus_sum)
            {
                // store for possible use
                m_pimpl->header_vector.push_back(std::move(*it));
                m_pimpl->m_blockchain.header_at(tmp_header.block_number - 1, tmp_header);
            }
        }

        if (lcb_found)
        {
            if (m_pimpl->block_vector.empty())
            {
                // nothing new! interrup connection
                sk.send(peerid, Drop());
                m_pimpl->remove_peer(peerid);
                m_pimpl->clear_state();
                return;
            }

            //3. request blockchain from found point
            BlockChainRequest blockchain_request;
            blockchain_request.blocks_from = m_pimpl->header_vector.rbegin()->block_number;
            blockchain_request.blocks_to = m_pimpl->header_vector.begin()->block_number;

            sk.send(peerid, blockchain_request);

            //TODO store request
        }
    }

    if (!found || !lcb_found)
    {
        // request more headers
        BlockHeaderRequest header_request;
        header_request.blocks_from = m_pimpl->header_vector.rbegin()->block_number - 1;
        header_request.blocks_to = header_request.blocks_from - 10;

        sk.send(peerid, header_request);

        //TODO store request
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
    to = to > from + 10 ? from + 10 : to;
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
    //2. check received blockchain validity
    BlockChainResponse blockchain_response;
    std::move(package).get(blockchain_response);

    // find previous block
    SignedBlock prev_signed_block;
    if (m_pimpl->block_vector.empty())
    {
        uint64_t number = (*m_pimpl->header_vector.rbegin()).block_number;

        if (number == 0)
        {
            // uzum en qcen! interrup connection
            sk.send(peerid, Drop());
            m_pimpl->remove_peer(peerid);
            m_pimpl->clear_state();
            return;
        }

        m_pimpl->m_blockchain.at(number - 1, prev_signed_block);
    }
    else
    {
        prev_signed_block = *m_pimpl->block_vector.rbegin();
    }

    Block prev_block;
    std::move(prev_signed_block.block_details).get(prev_block);

    bool bad_data = blockchain_response.signed_blocks.empty() ||
        blockchain_response.signed_blocks.size() > m_pimpl->header_vector.size() -
        m_pimpl->block_vector.size();

    for (auto it = m_pimpl->block_vector.begin(); !bad_data && it != m_pimpl->block_vector.end(); ++it)
    {
        SignedBlock signed_block = *it;
        //TODO check signature

        Block block;
        std::move(signed_block.block_details).get(block);

        bad_data = bad_data || block.block_header.block_number != prev_block.block_header.block_number + 1;

        //TODO check previous_hash
        //TODO check consensus_delta
        //TODO check consensus_sum
        //TODO check consensus_const

        //TODO check block transaction signatures

        prev_block = std::move(block);
    }

    if (bad_data)
    {
        sk.send(peerid, Drop());
        m_pimpl->remove_peer(peerid);
        m_pimpl->clear_state();
        return;
    }

    //3. add received blockchain to blocks_vector for future process
    for (auto it = m_pimpl->block_vector.begin(); it != m_pimpl->block_vector.end(); ++it)
        m_pimpl->block_vector.push_back(std::move(*it));

    if (m_pimpl->block_vector.size() < m_pimpl->header_vector.size())
    {
        BlockChainRequest blockchain_request;
        blockchain_request.blocks_from = prev_block.block_header.block_number + 1;
        blockchain_request.blocks_to = m_pimpl->header_vector.begin()->block_number;

        sk.send(peerid, blockchain_request);

        //TODO store request

        return;
    }

    //4. apply received chain
    vector<SignedBlock> revert_block_vector;
    uint64_t from = m_pimpl->header_vector.rbegin()->block_number;
    for (auto i = m_pimpl->m_blockchain.length() - 1; i >= from; --i)
    {
        SignedBlock signed_block;
        m_pimpl->m_blockchain.at(i, signed_block);

        revert_block_vector.push_back(std::move(signed_block));
    }

    revert_blocks(m_pimpl->m_blockchain.length() - from, m_pimpl);

    if(!insert_blocks(m_pimpl->block_vector, m_pimpl))
        if (!insert_blocks(revert_block_vector, m_pimpl))
            throw std::runtime_error("Something wrong happenes. Cant't insert back own chain!");
}