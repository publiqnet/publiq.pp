#pragma once

#include "common.hpp"
#include "http.hpp"

#include "state.hpp"
#include "documents.hpp"
#include "action_log.hpp"
#include "blockchain.hpp"
#include "storage.hpp"
#include "authority_manager.hpp"
#include "nodeid_service.hpp"
#include "node_synchronization.hpp"
#include "storage_node.hpp"
#include "inbox.hpp"
#include "config.hpp"

#include <belt.pp/ievent.hpp>
#include <belt.pp/socket.hpp>
#include <belt.pp/packet.hpp>
#include <belt.pp/utility.hpp>
#include <belt.pp/scope_helper.hpp>
#include <belt.pp/message_global.hpp>
#include <belt.pp/timer.hpp>
#include <belt.pp/direct_stream.hpp>

#include <mesh.pp/p2psocket.hpp>
#include <mesh.pp/cryptoutility.hpp>
#include <mesh.pp/sessionutility.hpp>

#include <boost/filesystem/path.hpp>
#include <boost/functional/hash.hpp>

#include <chrono>
#include <thread>
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
using beltpp::event_handler;
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

using fp_counts_per_channel_views =
uint64_t (*)(std::map<uint64_t, std::map<std::string, std::map<std::string, uint64_t>>> const& item_per_owner,
uint64_t block_number,
bool is_testnet);

using fp_content_unit_validate_check =
bool (*)(std::vector<std::string> const& content_unit_file_uris,
std::string& find_duplicate,
uint64_t block_number,
bool is_testnet);

inline uint64_t counts_per_channel_views(map<uint64_t, map<string, map<string, uint64_t>>> const& item_per_owner,
                                         uint64_t/* block_number*/,
                                         bool/* is_testnet*/)
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
inline bool content_unit_validate_check(std::vector<std::string> const& content_unit_file_uris,
                                        std::string& find_duplicate,
                                        uint64_t /*block_number*/,
                                        bool /*is_testnet*/)
{
    unordered_set<string> file_uris;
    for (auto const& file_uri : content_unit_file_uris)
    {
        auto insert_res = file_uris.insert(file_uri);
        if (false == insert_res.second)
        {
            find_duplicate = file_uri;
            return false;
        }
    }

    return true;
}

class node_internals
{
public:
    node_internals(string const& genesis_signed_block,
                   filesystem::path const& fs_blockchain,
                   filesystem::path const& fs_action_log,
                   filesystem::path const& fs_transaction_pool,
                   filesystem::path const& fs_state,
                   filesystem::path const& fs_authority_store,
                   filesystem::path const& fs_documents,
                   filesystem::path const& fs_storages,
                   filesystem::path const& fs_storage,
                   filesystem::path const& fs_inbox,
                   beltpp::ilog* _plogger_p2p,
                   beltpp::ilog* _plogger_node,
                   config& ref_config,
                   uint64_t freeze_before_block,
                   uint64_t revert_blocks_count,
                   uint64_t revert_actions_count,
                   bool resync,
                   coin const& mine_amount_threshhold,
                   std::vector<coin> const& block_reward_array,
                   detail::fp_counts_per_channel_views p_counts_per_channel_views,
                   detail::fp_content_unit_validate_check p_content_unit_validate_check,
                   beltpp::direct_channel& channel,
                   unique_ptr<event_handler>&& inject_eh,
                   unique_ptr<socket>&& inject_rpc_socket,
                   unique_ptr<socket>&& inject_p2p_socket)
        : plogger_p2p(_plogger_p2p)
        , plogger_node(_plogger_node)
        , m_ptr_eh(nullptr == inject_eh ?
                       beltpp::libsocket::construct_event_handler() :
                       std::move(inject_eh) )
        , m_ptr_p2p_socket(new meshpp::p2psocket(
                               meshpp::getp2psocket(*m_ptr_eh,
                                                    ref_config.get_p2p_bind_to_address(),
                                                    ref_config.get_p2p_connect_to_addresses(),
                                                    get_putl(),
                                                    _plogger_p2p,
                                                    ref_config.get_key(),
                                                    ref_config.discovery_server(),
                                                    std::move(inject_p2p_socket))
        ))
        , m_ptr_rpc_socket(nullptr == inject_rpc_socket ?
                               beltpp::libsocket::getsocket<rpc_sf>(*m_ptr_eh) :
                               std::move(inject_rpc_socket) )
        , m_ptr_direct_stream(beltpp::construct_direct_stream(node_peerid, *m_ptr_eh, channel))
        , m_sync_timer()
        , m_check_timer()
        , m_broadcast_timer()
        , m_cache_cleanup_timer()
        , m_summary_report_timer()
        , m_storage_sync_delay()
        , m_stuck_on_old_blockchain_timer()
        , m_blockchain(fs_blockchain)
        , m_action_log(fs_action_log, ref_config.action_log())
        , m_transaction_pool(fs_transaction_pool)
        , m_state(fs_state, *this)
        , m_documents(fs_documents, fs_storages)
        , m_authority_manager(fs_authority_store)
        , m_storage_controller(fs_storage)
        , m_inbox(fs_inbox)
        , all_sync_info(*this)
        , pconfig(&ref_config)
        , m_service_statistics_broadcast_triggered(false)
        , m_initialize(true)
        , m_freeze_before_block(freeze_before_block)
        , m_resync_blockchain(resync ? 10 : uint64_t(-1))
        , m_revert_blocks_count(revert_blocks_count)
        , m_revert_actions_count(revert_actions_count)
        , m_genesis_signed_block(genesis_signed_block)
        , m_mine_amount_threshhold(mine_amount_threshhold)
        , m_block_reward_array(block_reward_array)
        , pcounts_per_channel_views(nullptr != p_counts_per_channel_views ?
                                                   p_counts_per_channel_views :
                                                   &counts_per_channel_views)
        , pcontent_unit_validate_check(nullptr != p_content_unit_validate_check ?
                                                      p_content_unit_validate_check :
                                                      &content_unit_validate_check)
    {
        m_sync_timer.set(chrono::seconds(SYNC_TIMER));
        m_check_timer.set(chrono::seconds(CHECK_TIMER));
        m_broadcast_timer.set(chrono::seconds(BROADCAST_TIMER));
        m_cache_cleanup_timer.set(chrono::seconds(CACHE_CLEANUP_TIMER));
        m_summary_report_timer.set(chrono::seconds(SUMMARY_REPORT_TIMER));
        m_storage_sync_delay.set(chrono::seconds(2 * CACHE_CLEANUP_TIMER));
        m_stuck_on_old_blockchain_timer.set(chrono::seconds(BLOCK_MINE_DELAY));

        m_ptr_eh->set_timer(chrono::seconds(EVENT_TIMER));

        m_broadcast_timer.update();
        m_storage_sync_delay.update();

        if (false == pconfig->get_rpc_bind_to_address().local.empty())
            m_ptr_rpc_socket->listen(pconfig->get_rpc_bind_to_address());

        m_ptr_eh->add(*m_ptr_rpc_socket);
        m_ptr_eh->add(*m_ptr_p2p_socket);
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
        m_authority_manager.save();

        guard.dismiss();

        m_state.commit();
        m_documents.commit();
        m_blockchain.commit();
        m_action_log.commit();
        m_transaction_pool.commit();
        m_authority_manager.commit();
    }

    void discard()
    {
        m_state.discard();
        m_documents.discard();
        m_blockchain.discard();
        m_action_log.discard();
        m_transaction_pool.discard();
        m_authority_manager.discard();
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
        return m_state.get_balance(front_public_key().to_string(), state_layer::chain);
    }

    bool is_miner() const
    {
        bool result = (pconfig->get_node_type() == NodeType::blockchain) &&
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

    meshpp::public_key front_public_key() const
    {
        return pconfig->get_public_key();
    }

    meshpp::private_key front_private_key() const
    {
        return pconfig->get_key();
    }

    bool initialize();

    beltpp::ilog* plogger_p2p;
    beltpp::ilog* plogger_node;
    beltpp::event_handler_ptr m_ptr_eh;
    unique_ptr<meshpp::p2psocket> m_ptr_p2p_socket;
    beltpp::socket_ptr m_ptr_rpc_socket;
    beltpp::stream_ptr m_ptr_direct_stream;

    beltpp::timer m_sync_timer;
    beltpp::timer m_check_timer;
    beltpp::timer m_broadcast_timer;
    beltpp::timer m_cache_cleanup_timer;
    beltpp::timer m_summary_report_timer;
    beltpp::timer m_storage_sync_delay;
    beltpp::timer m_stuck_on_old_blockchain_timer;

    publiqpp::blockchain m_blockchain;
    publiqpp::action_log m_action_log;
    publiqpp::transaction_pool m_transaction_pool;
    publiqpp::state m_state;
    publiqpp::documents m_documents;
    publiqpp::authority_manager m_authority_manager;
    publiqpp::storage_controller m_storage_controller;
    publiqpp::inbox m_inbox;

    node_synchronization all_sync_info;
    detail::service_counter service_counter;

    publiqpp::nodeid_service m_nodeid_service;
    meshpp::session_manager<meshpp::nodeid_session_header> m_sync_sessions;
    meshpp::session_manager<meshpp::nodeid_session_header> m_nodeid_sessions;
    meshpp::session_manager<meshpp::session_header> m_sessions;

    unordered_set<beltpp::stream::peer_id> m_p2p_peers;
    transaction_cache m_transaction_cache;

    config* pconfig;

    bool m_service_statistics_broadcast_triggered;
    bool m_initialize;

    uint64_t m_freeze_before_block;
    uint64_t m_resync_blockchain;
    uint64_t m_revert_blocks_count;
    uint64_t m_revert_actions_count;

    string m_genesis_signed_block;

    coin const m_mine_amount_threshhold;
    std::vector<coin> const m_block_reward_array;
    fp_counts_per_channel_views pcounts_per_channel_views;
    fp_content_unit_validate_check pcontent_unit_validate_check;

    struct vote_info
    {
        coin stake;
        string block_hash;
        std::chrono::steady_clock::time_point tp;
    };

    unordered_map<string, vote_info> m_votes;
    unordered_map<string, string> m_nodeid_authorities;
    event_queue_manager m_event_queue;
};

}
}
