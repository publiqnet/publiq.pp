#pragma once

#include "common.hpp"
#include "http.hpp"

#include "state.hpp"
#include "documents.hpp"
#include "action_log.hpp"
#include "blockchain.hpp"
#include "nodeid_service.hpp"
#include "node_synchronization.hpp"

#include <belt.pp/event.hpp>
#include <belt.pp/socket.hpp>
#include <belt.pp/packet.hpp>
#include <belt.pp/utility.hpp>
#include <belt.pp/scope_helper.hpp>
#include <belt.pp/message_global.hpp>
#include <belt.pp/timer.hpp>

#include <mesh.pp/p2psocket.hpp>
#include <mesh.pp/cryptoutility.hpp>
#include <mesh.pp/sessionutility.hpp>

#include <boost/filesystem/path.hpp>

#include <map>
#include <chrono>
#include <memory>

using namespace BlockchainMessage;
namespace filesystem = boost::filesystem;

using beltpp::ip_address;
using beltpp::ip_destination;
using beltpp::socket;
using beltpp::packet;
using peer_id = socket::peer_id;

using std::map;
using std::pair;
using std::string;
using std::vector;
using std::unique_ptr;
using std::unordered_set;
using std::unordered_map;

namespace chrono = std::chrono;
using chrono::system_clock;
using chrono::steady_clock;

namespace publiqpp
{
    using rpc_sf = beltpp::socket_family_t<&http::message_list_load<&BlockchainMessage::message_list_load>>;

namespace detail
{

class packet_and_expiry
{
public:
    beltpp::packet packet;
    size_t expiry;
};

class task_table
{
public:
    void add(uint64_t task_id, packet& task_packet) 
    {
        task_map[task_id] = pair<system_clock::time_point, packet>(system_clock::now(), std::move(task_packet));
    };

    bool remove(uint64_t task_id, packet& task_packet)
    {
        auto task_it = task_map.find(task_id);

        if (task_it != task_map.end())
        {
            task_packet = std::move(task_it->second.second);

            task_map.erase(task_it);

            return true;
        }

        return false;
    }

    void clean() 
    {
        auto current_time = system_clock::now();

        auto it = task_map.begin();
        while (it != task_map.end())
        {
            chrono::seconds diff_seconds = chrono::duration_cast<chrono::seconds>(current_time - it->second.first);

            if (diff_seconds.count() < BLOCK_MINE_DELAY)
                ++it;
            else
                it = task_map.erase(it);
        }
    };

private:
    map<uint64_t, pair<system_clock::time_point, packet>> task_map;
};

class node_internals
{
public:
    node_internals(
        string const& genesis_signed_block,
        ip_address const & public_address,
        ip_address const& rpc_bind_to_address,
        ip_address const& slave_connect_to_address,
        ip_address const& p2p_bind_to_address,
        std::vector<ip_address> const& p2p_connect_to_addresses,
        filesystem::path const& fs_blockchain,
        filesystem::path const& fs_action_log,
        filesystem::path const& fs_transaction_pool,
        filesystem::path const& fs_state,
        filesystem::path const& fs_documents,
        beltpp::ilog* _plogger_p2p,
        beltpp::ilog* _plogger_node,
        meshpp::private_key const& pv_key,
        NodeType& n_type,
        bool log_enabled,
        bool transfer_only)
        : plogger_p2p(_plogger_p2p)
        , plogger_node(_plogger_node)
        , m_ptr_eh(new beltpp::event_handler())
        , m_ptr_p2p_socket(new meshpp::p2psocket(
                               meshpp::getp2psocket(*m_ptr_eh,
                                                    p2p_bind_to_address,
                                                    p2p_connect_to_addresses,
                                                    get_putl(),
                                                    _plogger_p2p,
                                                    pv_key)
        ))
        , m_ptr_rpc_socket(new beltpp::socket(
                               beltpp::getsocket<rpc_sf>(*m_ptr_eh)
                               ))
        , m_sync_timer()
        , m_check_timer()
        , m_summary_report_timer()
        , m_public_address(public_address)
        , m_rpc_bind_to_address(rpc_bind_to_address)
        , m_slave_connect_to_address(slave_connect_to_address)
        , m_blockchain(fs_blockchain)
        , m_action_log(fs_action_log, log_enabled)
        , m_transaction_pool(fs_transaction_pool)
        , m_state(fs_state)
        , m_documents(fs_documents)
        , m_node_type(n_type)
        , m_pv_key(pv_key)
        , m_pb_key(pv_key.get_public_key())
        , m_transfer_only(transfer_only)
        , all_sync_info(*this)
    {
        m_sync_timer.set(chrono::seconds(SYNC_TIMER));
        m_check_timer.set(chrono::seconds(CHECK_TIMER));
        m_broadcast_timer.set(chrono::seconds(BROADCAST_TIMER));
        m_reconnect_timer.set(chrono::seconds(RECONNECT_TIMER));
        m_cache_cleanup_timer.set(chrono::seconds(CACHE_CLEANUP_TIMER));
        m_summary_report_timer.set(chrono::seconds(SUMMARY_REPORT_TIMER));
        m_ptr_eh->set_timer(chrono::seconds(EVENT_TIMER));

        m_broadcast_timer.update();

        if (false == rpc_bind_to_address.local.empty())
            m_ptr_rpc_socket->listen(rpc_bind_to_address);

        m_ptr_eh->add(*m_ptr_rpc_socket);
        m_ptr_eh->add(*m_ptr_p2p_socket);

        if (m_blockchain.length() == 0)
            insert_genesis(genesis_signed_block);
        else
        {
            SignedBlock const& signed_block = m_blockchain.at(0);

            if (signed_block.to_string() != genesis_signed_block)
                throw std::runtime_error("the stored genesis is different from the one built in");
        }

        NodeType stored_node_type;
        if (m_state.get_role(m_pb_key.to_string(), stored_node_type) &&
            stored_node_type != m_node_type)
            throw std::runtime_error("the stored node role is different");

        m_slave_taskid = 0;

        load_transaction_cache();
    }

    void writeln_p2p(string const& value)
    {
        if (plogger_p2p)
            plogger_p2p->message(value);
    }

    void writeln_node(string const& value)
    {
        if (plogger_node)
            plogger_node->message(value);
    }

    void writeln_node_warning(string const& value)
    {
        if (plogger_node)
            plogger_node->warning(value);
    }

    void add_peer(socket::peer_id const& peerid)
    {
        pair<unordered_set<socket::peer_id>::iterator, bool> result =
            m_p2p_peers.insert(peerid);

        if (result.second == false)
            throw std::runtime_error("p2p peer already exists: " + peerid);
    }

    void remove_peer(socket::peer_id peerid)
    {
        if (0 == m_p2p_peers.erase(peerid))
            throw std::runtime_error("p2p peer not found to remove: " + peerid);
    }

    void reconnect_slave()
    {
        if (m_node_type != NodeType::blockchain && m_slave_peer.empty())
        {
            auto peers_list = m_ptr_rpc_socket->open(m_slave_connect_to_address);
            m_slave_peer_attempt = peers_list.front();
        }
    }

    void save(beltpp::on_failure& guard)
    {
        m_state.save();
        m_documents.save();
        m_blockchain.save();
        m_action_log.save();
        m_transaction_pool.save();

        guard.dismiss();

        m_state.commit();
        m_documents.commit();
        m_blockchain.commit();
        m_action_log.commit();
        m_transaction_pool.commit();
    }

    void discard()
    {
        m_state.discard();
        m_documents.discard();
        m_blockchain.discard();
        m_action_log.discard();
        m_transaction_pool.discard();
    }

    void load_transaction_cache()
    {
        writeln_node("Loading recent blocks to cache");

        std::chrono::system_clock::time_point time_signed_head;

        uint64_t block_count = m_blockchain.length();
        for (uint64_t block_index = block_count - 1;
             block_index < block_count;
             --block_index)
        {
            SignedBlock const& signed_block = m_blockchain.at(block_index);

            Block const& block = signed_block.block_details;

            std::chrono::system_clock::time_point time_signed =
                    std::chrono::system_clock::from_time_t(block.header.time_signed.tm);
            if (block_index == block_count - 1)
                time_signed_head = time_signed;
            else if (time_signed_head - time_signed >
                     std::chrono::hours(TRANSACTION_MAX_LIFETIME_HOURS) +
                     std::chrono::seconds(NODES_TIME_SHIFT))
                break; //   because all transactions in this block must be expired

            for (auto& item : block.signed_transactions)
            {
                string key = meshpp::hash(item.to_string());
                m_transaction_cache[key] = system_clock::from_time_t(item.transaction_details.creation.tm);
            }
        }

        for (size_t index = 0; index != m_transaction_pool.length(); ++index)
        {
            SignedTransaction const& item = m_transaction_pool.at(index);
            string key = meshpp::hash(item.to_string());
            m_transaction_cache[key] = system_clock::from_time_t(item.transaction_details.creation.tm);
        }
    }

    void clean_transaction_cache()
    {
        BlockHeader const& current_header = m_blockchain.last_header();

        system_clock::time_point cur_time_point = system_clock::from_time_t(current_header.time_signed.tm);

        auto it = m_transaction_cache.begin();
        while (it != m_transaction_cache.end())
        {
            if (cur_time_point - it->second >
                std::chrono::hours(TRANSACTION_MAX_LIFETIME_HOURS) +
                std::chrono::seconds(NODES_TIME_SHIFT))
                //  we don't need to keep in hash the transactions that are definitely expired
                it = m_transaction_cache.erase(it);
            else
                ++it;
        }
    }

    static
    uint64_t calc_delta(string const& key, uint64_t const& amount, string const& prev_hash, uint64_t const& cons_const)
    {
        uint64_t dist = meshpp::distance(meshpp::hash(key), prev_hash);
        uint64_t delta = amount * DIST_MAX / ((dist + 1) * cons_const);

        if (delta > DELTA_MAX)
            delta = DELTA_MAX;

        return delta;
    }

    BlockchainMessage::Coin get_balance() const
    {
        return m_state.get_balance(m_pb_key.to_string());
    }

    bool is_miner() const
    {
        bool result = (m_node_type == NodeType::blockchain) &&
                      (coin(get_balance()) >= MINE_AMOUNT_THRESHOLD);

        return result;
    }

    void insert_genesis(string const& genesis_signed_block)
    {
        if (m_blockchain.length() > 0)
            return;

        SignedBlock signed_block;

        signed_block.from_string(genesis_signed_block);

        beltpp::on_failure guard([&] { discard(); });

        // apply rewards to state and action_log
        for (auto const& item : signed_block.block_details.rewards)
            m_state.increase_balance(item.to, item.amount);

        // insert to blockchain and action_log
        m_blockchain.insert(signed_block);
        m_action_log.log_block(signed_block);

        save(guard);
    }

    beltpp::ilog* plogger_p2p;
    beltpp::ilog* plogger_node;
    unique_ptr<beltpp::event_handler> m_ptr_eh;
    unique_ptr<meshpp::p2psocket> m_ptr_p2p_socket;
    unique_ptr<beltpp::socket> m_ptr_rpc_socket;

    beltpp::timer m_sync_timer;
    beltpp::timer m_check_timer;
    beltpp::timer m_broadcast_timer;
    beltpp::timer m_reconnect_timer;
    beltpp::timer m_cache_cleanup_timer;
    beltpp::timer m_summary_report_timer;

    beltpp::ip_address m_public_address;
    beltpp::ip_address m_rpc_bind_to_address;
    beltpp::ip_address m_slave_connect_to_address;

    publiqpp::blockchain m_blockchain;
    publiqpp::action_log m_action_log;
    publiqpp::transaction_pool m_transaction_pool;
    publiqpp::state m_state;
    publiqpp::documents m_documents;

    publiqpp::nodeid_service m_nodeid_service;
    meshpp::session_manager m_sessions;

    beltpp::isocket::peer_id m_slave_peer;
    beltpp::isocket::peer_id m_slave_peer_attempt;

    unordered_set<beltpp::isocket::peer_id> m_p2p_peers;
    unordered_map<string, system_clock::time_point> m_transaction_cache;

    NodeType m_node_type;
    uint64_t m_slave_taskid;
    task_table m_slave_tasks;
    meshpp::private_key m_pv_key;
    meshpp::public_key m_pb_key;

    bool m_transfer_only;

    node_synchronization all_sync_info;
};

}
}
