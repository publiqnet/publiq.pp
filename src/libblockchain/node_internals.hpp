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
#include <map>
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
using std::map;
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

class service_counter
{
public:
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
    class service_unit_counter
    {
    public:
        string session_id;
        system_clock::time_point time_point;
        uint64_t seconds;

        bool operator == (service_unit_counter const& other) const
        {
            return (session_id == other.session_id &&
                    time_point == other.time_point &&
                    seconds == other.seconds);
        }
    };
private:
    struct service_unit_hash
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
    struct service_unit_counter_hash
    {
        size_t operator()(service_unit_counter const& value) const noexcept
        {
            size_t hash_value = 0xdeadbeef;
            boost::hash_combine(hash_value, value.session_id);
            boost::hash_combine(hash_value, system_clock::to_time_t(value.time_point));
            boost::hash_combine(hash_value, value.seconds);
            return hash_value;
        }
    };

    using service_unit_counter_map = unordered_map<service_unit_counter, bool, service_unit_counter_hash>;
public:
    void served(service_unit const& unit,
                service_unit_counter const& unit_counter)
    {
        auto now = system_clock::now();

        if (unit_counter.time_point > now + chrono::seconds(NODES_TIME_SHIFT))
            throw std::logic_error("unit_counter.time_point > now + chrono::seconds(NODES_TIME_SHIFT)");
        if (unit_counter.time_point + chrono::seconds(unit_counter.seconds) <= now - chrono::seconds(NODES_TIME_SHIFT))
            throw std::logic_error("unit_counter.time_point + chrono::seconds(unit_counter.seconds) <= now - chrono::seconds(NODES_TIME_SHIFT)");

        auto insert_res = m_served.insert({unit, service_unit_counter_map()});
        insert_res.first->second.insert({unit_counter, false});
    }

    ServiceStatistics take_statistics_info()
    {
        struct index_helper
        {
            string content_unit_uri;
            string file_uri;

            bool operator == (index_helper const& other) const
            {
                return (content_unit_uri == other.content_unit_uri &&
                        file_uri == other.file_uri);
            }
        };
        struct hash_index_helper
        {
            size_t operator()(index_helper const& value) const noexcept
            {
                size_t hash_value = 0xdeadbeef;
                boost::hash_combine(hash_value, value.file_uri);
                boost::hash_combine(hash_value, value.content_unit_uri);
                return hash_value;
            }
        };

        unordered_map<index_helper, size_t, hash_index_helper> index;
        ServiceStatistics service_statistics;

        auto now = system_clock::now();

        auto it = m_served.begin();
        while (it != m_served.end())
        {
            auto const& unit = it->first;
            auto& unit_counter_map = it->second;

            ServiceStatisticsCount stat_count;
            stat_count.peer_address = unit.peer_address;

            auto unit_counter_it = unit_counter_map.begin();
            while (unit_counter_it != unit_counter_map.end())
            {
                auto const& unit_counter = unit_counter_it->first;

                bool expired = false;
                if (unit_counter.time_point + chrono::seconds(unit_counter.seconds) <= now - chrono::seconds(NODES_TIME_SHIFT))
                    expired = true;
                bool& counted = unit_counter_it->second;

                if (false == counted)
                {
                    ++stat_count.count;
                    counted = true;
                }

                if (expired)
                    unit_counter_it = unit_counter_map.erase(unit_counter_it);
                else
                    ++unit_counter_it;
            }

            if (stat_count.count)
            {
                auto const& file_uri = unit.file_uri;
                auto const& content_unit_uri = unit.content_unit_uri;

                ServiceStatisticsFile* pstat_file = nullptr;

                auto insert_result = index.insert({
                                                      {content_unit_uri, file_uri},
                                                      service_statistics.file_items.size()
                                                  });
                if (false == insert_result.second)
                {
                    pstat_file = &service_statistics.file_items[insert_result.first->second];
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
                stat_file.count_items.push_back(stat_count);
            }

            if (unit_counter_map.empty())
                it = m_served.erase(it);
            else
                ++it;
        };

        return service_statistics;
    }

private:
    unordered_map<service_unit, service_unit_counter_map, service_unit_hash> m_served;
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

inline coin coin_from_fractions(uint64_t fractions)
{
    coin result(0, 1);
    result *= fractions;
    return result;
}

using fp_counts_per_channel_views =
uint64_t (*)(std::map<uint64_t, std::map<std::string, std::map<std::string, uint64_t>>> const& item_per_owner,
uint64_t block_number,
bool is_testnet);

inline uint64_t counts_per_channel_views(map<uint64_t, map<string, map<string, uint64_t>>> const& item_per_owner,
                                         uint64_t /*block_number*/,
                                         bool /*is_testnet*/)
{
    uint64_t count = 0;
    for (auto const& item_per_content_id : item_per_owner)
    {
        uint64_t max_count_per_content_id = 0;
        for (auto const& item_per_unit : item_per_content_id.second)
        for (auto const& item_per_file : item_per_unit.second)
            max_count_per_content_id = std::max(max_count_per_content_id, item_per_file.second);

        count += max_count_per_content_id;
    }

    return count;
}

class node_internals
{
public:
    node_internals(string const& genesis_signed_block,
                   ip_address const& public_address,
                   ip_address const& public_ssl_address,
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
                   uint64_t fractions,
                   uint64_t freeze_before_block,
                   bool log_enabled,
                   bool transfer_only,
                   bool testnet,
                   coin const& mine_amount_threshhold,
                   std::vector<coin> const& block_reward_array,
                   std::chrono::steady_clock::duration const& sync_delay,
                   detail::fp_counts_per_channel_views p_counts_per_channel_views)
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
        , m_public_ssl_address(public_ssl_address)
        , m_rpc_bind_to_address(rpc_bind_to_address)
        , m_blockchain(fs_blockchain)
        , m_action_log(fs_action_log, log_enabled)
        , m_transaction_pool(fs_transaction_pool)
        , m_state(fs_state, *this)
        , m_documents(fs_documents, fs_storages)
        , all_sync_info(*this)
        , m_node_type(n_type)
        , m_fee_transactions(std::move(coin_from_fractions(fractions)))
        , m_pv_key(pv_key)
        , m_pb_key(pv_key.get_public_key())
        , m_testnet(testnet)
        , m_transfer_only(transfer_only)
        , m_service_statistics_broadcast_triggered(false)
        , m_freeze_before_block(freeze_before_block)
        , m_mine_amount_threshhold(mine_amount_threshhold)
        , m_block_reward_array(block_reward_array)
        , pcounts_per_channel_views(nullptr != p_counts_per_channel_views ?
                                                   p_counts_per_channel_views :
                                                   &counts_per_channel_views)
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
        m_action_log.log_block(signed_block, map<string, map<string, uint64_t>>(), map<string, coin>());

        save(guard);
    }

    bool blockchain_updated()
    {
        auto const last_header = m_blockchain.last_header();

        chrono::system_clock::duration last_block_age =
            chrono::system_clock::now() -
            chrono::system_clock::from_time_t(last_header.time_signed.tm);

        // maybe need to change this funtion to tell when the block is even more older

        return last_block_age < chrono::seconds(BLOCK_MINE_DELAY);
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
    beltpp::ip_address m_public_ssl_address;
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
    coin m_fee_transactions;
    meshpp::private_key m_pv_key;
    meshpp::public_key m_pb_key;

    bool m_testnet;
    bool m_transfer_only;
    bool m_service_statistics_broadcast_triggered;

    uint64_t m_freeze_before_block;

    coin const m_mine_amount_threshhold;
    std::vector<coin> const m_block_reward_array;
    fp_counts_per_channel_views pcounts_per_channel_views;
    unordered_map<string, unordered_map<string, bool>> map_channel_to_file_uris;

    struct vote_info
    {
        coin stake;
        string block_hash;
        std::chrono::steady_clock::time_point tp;
    };

    unordered_map<string, vote_info> m_votes;
};

}
}
