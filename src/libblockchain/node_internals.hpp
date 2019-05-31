#pragma once

#include "common.hpp"
#include "http.hpp"

#include "state.hpp"
#include "documents.hpp"
#include "action_log.hpp"
#include "blockchain.hpp"
#include "nodeid_service.hpp"
#include "node_synchronization.hpp"
#include "storage_node.hpp"

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
#include <boost/functional/hash.hpp>

#include <chrono>
#include <memory>
#include <utility>
#include <vector>
#include <unordered_map>
#include <unordered_set>

using namespace BlockchainMessage;
namespace filesystem = boost::filesystem;

using beltpp::ip_address;
using beltpp::ip_destination;
using beltpp::socket;
using beltpp::packet;
using peer_id = socket::peer_id;

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

/*class packet_and_expiry
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
    }

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
};*/

class service_counter
{
    class service_unit
    {
    public:
        string file_uri;
        string content_unit_uri;
        string peer_address;

        bool operator == (service_unit const& other) const
        {
            return (file_uri == other.file_uri &&
                    content_unit_uri == other.content_unit_uri &&
                    peer_address == other.peer_address);
        }
    };

    struct hash
    {
        size_t operator()(service_unit const& value) const noexcept
        {
            size_t hash_value = 0xdeadbeef;
            boost::hash_combine(hash_value, value.file_uri);
            boost::hash_combine(hash_value, value.content_unit_uri);
            boost::hash_combine(hash_value, value.peer_address);
            return hash_value;
        }
    };
public:
    void served(string const& content_unit_uri,
                string const& file_uri,
                string const& peer_address)
    {
        service_unit unit;
        unit.content_unit_uri = content_unit_uri;
        unit.file_uri = file_uri;
        unit.peer_address = peer_address;

        ++m_served[unit];
    }

    ServiceStatistics take_statistics_info()
    {
        ServiceStatistics service_statistics;

        unordered_map<service_unit, size_t, hash> index;

        for (auto const& item : m_served)
        {
            auto const& file_uri = item.first.file_uri;
            auto const& content_unit_uri = item.first.content_unit_uri;

            ServiceStatisticsFile* pstat_file = nullptr;

            service_unit index_key;
            index_key.content_unit_uri = content_unit_uri;
            index_key.file_uri = file_uri;

            auto insert_result = index.insert(std::make_pair(
                                       index_key,
                                       service_statistics.file_items.size()));
            if (false == insert_result.second)
            {
                auto it = insert_result.first;
                pstat_file = &service_statistics.file_items[it->second];
            }
            else
            {
                ServiceStatisticsFile stat_file_local;
                stat_file_local.file_uri = file_uri;
                stat_file_local.unit_uri = content_unit_uri;
                service_statistics.file_items.push_back(stat_file_local);

                pstat_file = &service_statistics.file_items.back();
            }

            ServiceStatisticsFile& stat_file = *pstat_file;

            ServiceStatisticsCount stat_count;
            stat_count.peer_address = item.first.peer_address;
            stat_count.count = item.second;

            stat_file.count_items.push_back(stat_count);
        }

        m_served.clear();

        return service_statistics;
    }

private:
    unordered_map<service_unit, uint64_t, hash> m_served;
};

class transaction_cache
{
public:
    bool add_chain(SignedTransaction const& signed_transaction)
    {
        string key = meshpp::hash(signed_transaction.to_string());

        if (data.count(key))
            return false;

        if (signed_transaction.authorizations.size() > 1)
        {
            SignedTransaction st = signed_transaction;
            auto authorizations = st.authorizations;

            for (auto const& authorization : authorizations)
            {
                st.authorizations = {authorization};
                string key_sub = meshpp::hash(st.to_string());

                if (data.count(key_sub))
                    return false;
            }

            for (auto const& authorization : authorizations)
            {
                st.authorizations = {authorization};
                string key_sub = meshpp::hash(st.to_string());
                auto insert_result = data.insert({key_sub, {true, system_clock::from_time_t(signed_transaction.transaction_details.creation.tm)}});
                assert(insert_result.second);
                B_UNUSED(insert_result);
            }
        }

        auto insert_result = data.insert({key, {true, system_clock::from_time_t(signed_transaction.transaction_details.creation.tm)}});
        assert(insert_result.second);
        B_UNUSED(insert_result);

        return true;
    }
    void erase_chain(SignedTransaction const& signed_transaction)
    {
        string key = meshpp::hash(signed_transaction.to_string());
        auto count = data.erase(key);
        B_UNUSED(count);
        /*if (0 == count)
            throw std::logic_error("inconsistent transaction cache");*/

        if (signed_transaction.authorizations.size() > 1)
        {
            SignedTransaction st = signed_transaction;
            auto authorizations = st.authorizations;

            for (auto const& authorization : authorizations)
            {
                st.authorizations = {authorization};
                string key_sub = meshpp::hash(st.to_string());
                count = data.erase(key_sub);
                /*if (0 == count)
                    throw std::logic_error("inconsistent transaction cache");*/
            }
        }
    }

    bool add_pool(SignedTransaction const& signed_transaction,
                  bool complete)
    {
        string key = meshpp::hash(signed_transaction.to_string());

        auto insert_result = data.insert({key, {complete, system_clock::from_time_t(signed_transaction.transaction_details.creation.tm)}});
        if (false == insert_result.second)
            return false;

        return true;
    }

    bool erase_pool(SignedTransaction const& signed_transaction)
    {
        bool complete = false;
        string key = meshpp::hash(signed_transaction.to_string());
        auto it = data.find(key);
        /*if (it == data.end())
            throw std::logic_error("inconsistent transaction cache");*/
        if (it != data.end())
        {
            if (it->second.complete)
                complete = true;

            data.erase(it);
        }

        return complete;
    }

    void clean(system_clock::time_point const& tp)
    {
        auto it = data.begin();
        while (it != data.end())
        {
            if (tp - it->second.tp >
                std::chrono::hours(TRANSACTION_MAX_LIFETIME_HOURS) +
                std::chrono::seconds(NODES_TIME_SHIFT))
                //  we don't need to keep in hash the transactions that are definitely expired
                it = data.erase(it);
            else
                ++it;
        }
    }

    bool contains(SignedTransaction const& signed_transaction) const
    {
        string key = meshpp::hash(signed_transaction.to_string());
        return data.count(key) > 0;
    }

    void backup()
    {
        data_backup = data;
    }
    void restore() noexcept
    {
        data = std::move(data_backup);
    }
protected:
    class data_type
    {
    public:
        bool complete;
        system_clock::time_point tp;
    };
    unordered_map<string, data_type> data;
    unordered_map<string, data_type> data_backup;
};

class node_internals
{
public:
    node_internals(string const& genesis_signed_block,
                   ip_address const & public_address,
                   ip_address const& rpc_bind_to_address,
                   ip_address const& p2p_bind_to_address,
                   std::vector<ip_address> const& p2p_connect_to_addresses,
                   filesystem::path const& fs_blockchain,
                   filesystem::path const& fs_action_log,
                   filesystem::path const& fs_transaction_pool,
                   filesystem::path const& fs_state,
                   filesystem::path const& fs_documents,
                   filesystem::path const& fs_storages,
                   beltpp::ilog* _plogger_p2p,
                   beltpp::ilog* _plogger_node,
                   meshpp::private_key const& pv_key,
                   NodeType& n_type,
                   bool log_enabled,
                   bool transfer_only,
                   bool testnet,
                   coin const& mine_amount_threshhold,
                   std::vector<coin> const& block_reward_array,
                   std::chrono::steady_clock::duration const& sync_delay)
        : m_slave_node(nullptr)
        , plogger_p2p(_plogger_p2p)
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
        , m_sync_delay()
        , m_public_address(public_address)
        , m_rpc_bind_to_address(rpc_bind_to_address)
        , m_blockchain(fs_blockchain)
        , m_action_log(fs_action_log, log_enabled)
        , m_transaction_pool(fs_transaction_pool)
        , m_state(fs_state, *this)
        , m_documents(fs_documents, fs_storages)
        , all_sync_info(*this)
        , m_node_type(n_type)
        , m_pv_key(pv_key)
        , m_pb_key(pv_key.get_public_key())
        , m_transfer_only(transfer_only)
        , m_testnet(testnet)
        , m_mine_amount_threshhold(mine_amount_threshhold)
        , m_block_reward_array(block_reward_array)
    {
        m_sync_timer.set(chrono::seconds(SYNC_TIMER));
        m_check_timer.set(chrono::seconds(CHECK_TIMER));
        m_broadcast_timer.set(chrono::seconds(BROADCAST_TIMER));
        m_cache_cleanup_timer.set(chrono::seconds(CACHE_CLEANUP_TIMER));
        m_summary_report_timer.set(chrono::seconds(SUMMARY_REPORT_TIMER));
        m_sync_delay.set(sync_delay, true);

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
            SignedBlock signed_block_hardcode;
            signed_block_hardcode.from_string(genesis_signed_block);

            if (signed_block.to_string() != signed_block_hardcode.to_string())
                throw std::runtime_error("the stored genesis is different from the one built in");
        }

        NodeType stored_node_type;
        if (m_state.get_role(m_pb_key.to_string(), stored_node_type) &&
            stored_node_type != m_node_type)
            throw std::runtime_error("the stored node role is different");

        load_transaction_cache(*this);
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
        writeln_node("remove peer: " + detail::peer_short_names(peerid));
        m_sync_sessions.remove(peerid);
        m_nodeid_sessions.remove(peerid);
        m_sessions.remove(peerid);
        if (0 == m_p2p_peers.erase(peerid))
            throw std::runtime_error("p2p peer not found to remove: " + peerid);
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

    void clean_transaction_cache()
    {
        BlockHeader const& current_header = m_blockchain.last_header();

        system_clock::time_point cur_time_point = system_clock::from_time_t(current_header.time_signed.tm);

        m_transaction_cache.clean(cur_time_point);
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
        return m_state.get_balance(m_pb_key.to_string(), state_layer::chain);
    }

    bool is_miner() const
    {
        bool result = (m_node_type == NodeType::blockchain) &&
                      (coin(get_balance()) >= m_mine_amount_threshhold);

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
            m_state.increase_balance(item.to, item.amount, state_layer::chain);

        // insert to blockchain and action_log
        m_blockchain.insert(signed_block);
        m_action_log.log_block(signed_block);

        save(guard);
    }

    storage_node* m_slave_node;
    beltpp::ilog* plogger_p2p;
    beltpp::ilog* plogger_node;
    unique_ptr<beltpp::event_handler> m_ptr_eh;
    unique_ptr<meshpp::p2psocket> m_ptr_p2p_socket;
    unique_ptr<beltpp::socket> m_ptr_rpc_socket;

    beltpp::timer m_sync_timer;
    beltpp::timer m_check_timer;
    beltpp::timer m_broadcast_timer;
    beltpp::timer m_cache_cleanup_timer;
    beltpp::timer m_summary_report_timer;
    beltpp::timer m_sync_delay;

    beltpp::ip_address m_public_address;
    beltpp::ip_address m_rpc_bind_to_address;

    publiqpp::blockchain m_blockchain;
    publiqpp::action_log m_action_log;
    publiqpp::transaction_pool m_transaction_pool;
    publiqpp::state m_state;
    publiqpp::documents m_documents;

    node_synchronization all_sync_info;
    detail::service_counter service_counter;

    publiqpp::nodeid_service m_nodeid_service;
    meshpp::session_manager<meshpp::nodeid_session_header> m_sync_sessions;
    meshpp::session_manager<meshpp::nodeid_session_header> m_nodeid_sessions;
    meshpp::session_manager<meshpp::session_header> m_sessions;

    unordered_set<beltpp::isocket::peer_id> m_p2p_peers;
    transaction_cache m_transaction_cache;

    NodeType m_node_type;
    meshpp::private_key m_pv_key;
    meshpp::public_key m_pb_key;

    bool m_transfer_only;
    bool m_testnet;

    coin const m_mine_amount_threshhold;
    std::vector<coin> const m_block_reward_array;
};

}
}
