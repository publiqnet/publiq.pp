#include "node_internals.hpp"
#include "common.hpp"
#include "communication_p2p.hpp"

namespace publiqpp
{
namespace detail
{

    bool node_internals::initialize()
    {
        bool stop_check = false;;

        if (m_revert_blocks)
        {
            m_transaction_cache.backup();
            beltpp::on_failure guard([this]
            {
                discard();
                m_transaction_cache.restore();
            });

            //  revert transactions from pool
            vector<SignedTransaction> pool_transactions = revert_pool(system_clock::to_time_t(system_clock::now()), *this);

            //  revert last block
            //  calculate back
            SignedBlock const& signed_block = m_blockchain.at(m_blockchain.last_header().block_number);
            m_blockchain.remove_last_block();
            m_action_log.revert();

            Block const& block = signed_block.block_details;

            map<string, map<string, uint64_t>> unit_uri_view_counts;
            map<string, coin> unit_sponsor_applied;
            // verify block rewards before reverting, this also reclaims advertisement coins
            if (check_rewards(block,
                              signed_block.authorization.address,
                              rewards_type::revert,
                              *this,
                              unit_uri_view_counts,
                              unit_sponsor_applied))
                writeln_node("Last (" + std::to_string(block.header.block_number) + ") block rewards reverting error!");

            B_UNUSED(unit_uri_view_counts);
            B_UNUSED(unit_sponsor_applied);

            // decrease all reward amounts from balances and revert reward
            for (auto it = block.rewards.crbegin(); it != block.rewards.crend(); ++it)
                m_state.decrease_balance(it->to, it->amount, state_layer::chain);

            // calculate back transactions
            for (auto it = block.signed_transactions.crbegin(); it != block.signed_transactions.crend(); ++it)
            {
                revert_transaction(*it, *this, signed_block.authorization.address);
                m_transaction_cache.erase_chain(*it);
            }

            writeln_node("Last (" + std::to_string(block.header.block_number) + ") block reverted");

            save(guard);

            m_revert_blocks = false;
            stop_check = true;
        }
        else if (m_resync_blockchain != uint64_t(-1))
        {
            if (m_resync_blockchain)
            {
                writeln_node("blockchain data cleanup will start in: " + std::to_string(m_resync_blockchain) + " seconds");
                std::this_thread::sleep_for(std::chrono::seconds(1));
                --m_resync_blockchain;
            }
            else
            {
                beltpp::on_failure guard([this]
                {
                    discard();
                });

                m_state.clear();
                m_documents.clear();
                m_blockchain.clear();
                m_action_log.clear();
                m_transaction_pool.clear();

                save(guard);

                m_resync_blockchain = uint64_t(-1);
                writeln_node("blockchain data cleaned up");
            }
        }
        else
        {
            if (m_blockchain.length() == 0)
                insert_genesis(m_genesis_signed_block);
            else
            {
                SignedBlock const& signed_block = m_blockchain.at(0);
                SignedBlock signed_block_hardcode;
                signed_block_hardcode.from_string(m_genesis_signed_block);

                if (signed_block.to_string() != signed_block_hardcode.to_string())
                    throw std::runtime_error("the stored genesis is different from the one built in");
            }

            NodeType stored_node_type;
            if (m_state.get_role(m_pb_key.to_string(), stored_node_type) &&
                stored_node_type != m_node_type)
                throw std::runtime_error("the stored node role is different");

            load_transaction_cache(*this);

            m_initialize = false;
        }

        return stop_check;
    }

}
}
