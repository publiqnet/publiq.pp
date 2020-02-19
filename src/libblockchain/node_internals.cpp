#include "node_internals.hpp"
#include "common.hpp"
#include "communication_p2p.hpp"
#include "message.tmpl.hpp"

namespace publiqpp
{
namespace detail
{
bool node_internals::initialize()
{
    bool stop_check = false;

    if (m_revert_blocks)
    {
        // m_transaction_cache management is useless, the program is going to stop soon

//        m_transaction_cache.backup();
        beltpp::on_failure guard([this]
        {
            discard();
//            m_transaction_cache.restore();
        });

        //  revert transactions from pool
        load_transaction_cache(*this, true);
        revert_pool(system_clock::to_time_t(system_clock::now()), *this);

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
//        {
            revert_transaction(*it, *this, signed_block.authorization.address);
//            m_transaction_cache.erase_chain(*it);
//        }

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

        load_transaction_cache(*this, false);

        m_initialize = false;
    }

    return stop_check;
}

wait_result_item node_internals::wait_and_receive_one()
{
    auto& wait_result = m_wait_result.m_wait_result;

    if (wait_result == beltpp::event_handler::wait_result::nothing)
    {
        assert(m_wait_result.on_demand_packets.empty());
        if (false == m_wait_result.on_demand_packets.empty())
            throw std::logic_error("false == m_wait_result.on_demand_packets.empty()");
        assert(m_wait_result.event_packets.empty());
        if (false == m_wait_result.event_packets.empty())
            throw std::logic_error("false == m_wait_result.event_packets.empty()");

        unordered_set<beltpp::ievent_item const*> wait_sockets;

        wait_result = m_ptr_eh->wait(wait_sockets);

        if (wait_result & beltpp::event_handler::event)
        {
            for (auto& pevent_item : wait_sockets)
            {
                wait_result_item::interface_type it = wait_result_item::interface_type::rpc;
                if (pevent_item == &m_ptr_p2p_socket->worker())
                    it = wait_result_item::interface_type::p2p;

                beltpp::isocket* psk = nullptr;
                if (pevent_item == &m_ptr_p2p_socket->worker())
                    psk = m_ptr_p2p_socket.get();
                else if (pevent_item == m_ptr_rpc_socket.get())
                    psk = m_ptr_rpc_socket.get();

                if (nullptr == psk)
                    throw std::logic_error("event handler behavior");

                beltpp::socket::packets received_packets;
                beltpp::socket::peer_id peerid;
                received_packets = psk->receive(peerid);

                auto insert_res = m_wait_result.event_packets.insert(std::make_pair(it,
                                                                                    std::make_pair(peerid,
                                                                                                   std::move(received_packets))));
                assert(insert_res.second);
                if (false == insert_res.second)
                    throw std::logic_error("auto insert_res = m_wait_result.event_packets.insert({it, std::move(received_packets)});");
            }
        }

        /*if (wait_result & beltpp::event_handler::timer_out)
        {
        }*/

        if (m_slave_node && (wait_result & beltpp::event_handler::on_demand))
            m_wait_result.on_demand_packets = m_slave_node->receive();
    }

    auto result = wait_result_item::empty_result();

    if (wait_result & beltpp::event_handler::event)
    {
        if (false == m_wait_result.event_packets.empty())
        {
            auto it = m_wait_result.event_packets.begin();

            if (false == it->second.second.empty())
            {
                auto interface_type = it->first;
                auto packet = std::move(it->second.second.front());
                auto peerid = it->second.first;

                it->second.second.pop_front();

                result = wait_result_item::event_result(interface_type, peerid, std::move(packet));
            }

            if (it->second.second.empty())
                m_wait_result.event_packets.erase(it);
        }

        if (m_wait_result.event_packets.empty())
            wait_result = beltpp::event_handler::wait_result(wait_result & ~beltpp::event_handler::event);

        return result;
    }

    if (wait_result & beltpp::event_handler::timer_out)
    {
        wait_result = beltpp::event_handler::wait_result(wait_result & ~beltpp::event_handler::timer_out);
        result = wait_result_item::timer_result();
        return result;
    }

    if (wait_result & beltpp::event_handler::on_demand)
    {
        if (false == m_wait_result.on_demand_packets.empty())
        {
            auto packet = std::move(m_wait_result.on_demand_packets.front());
            m_wait_result.on_demand_packets.pop_front();

            result = wait_result_item::on_demand_result(std::move(packet));
        }

        if (m_wait_result.on_demand_packets.empty())
            wait_result = beltpp::event_handler::wait_result(wait_result & ~beltpp::event_handler::on_demand);

        return result;
    }

    return result;
}
}
}
