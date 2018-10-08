#pragma once

#include "common.hpp"
#include "http.hpp"

#include "state.hpp"
#include "storage.hpp"
#include "action_log.hpp"
#include "blockchain.hpp"

#include <belt.pp/event.hpp>
#include <belt.pp/socket.hpp>
#include <belt.pp/packet.hpp>
#include <belt.pp/utility.hpp>
#include <belt.pp/scope_helper.hpp>
#include <belt.pp/message_global.hpp>
#include <belt.pp/timer.hpp>

#include <mesh.pp/p2psocket.hpp>
#include <mesh.pp/cryptoutility.hpp>

#include <boost/filesystem/path.hpp>

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

using rpc_sf = beltpp::socket_family_t<&http::message_list_load>;

namespace detail
{

class packet_and_expiry
{
public:
    beltpp::packet packet;
    size_t expiry;
};

class node_internals
{
public:
    node_internals(ip_address const& rpc_bind_to_address,
        ip_address const& p2p_bind_to_address,
        std::vector<ip_address> const& p2p_connect_to_addresses,
        filesystem::path const& fs_blockchain,
        filesystem::path const& fs_action_log,
        filesystem::path const& fs_storage,
        filesystem::path const& fs_transaction_pool,
        filesystem::path const& fs_state,
        beltpp::ilog* _plogger_p2p,
        beltpp::ilog* _plogger_node,
        meshpp::private_key const& pv_key,
        bool log_enabled)
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
        , m_blockchain(fs_blockchain)
        , m_action_log(fs_action_log, log_enabled)
        , m_storage(fs_storage)
        , m_transaction_pool(fs_transaction_pool)
        , m_state(fs_state)
        , m_pv_key(pv_key)
    {
        m_sync_timer.set(chrono::seconds(SYNC_TIMER));
        m_check_timer.set(chrono::seconds(CHECK_TIMER));
        m_broadcast_timer.set(chrono::seconds(BROADCAST_TIMER));
        m_cache_cleanup_timer.set(chrono::seconds(CACHE_CLEANUP_TIMER));
        m_summary_report_timer.set(chrono::seconds(SUMMARY_REPORT_TIMER));
        m_ptr_eh->set_timer(chrono::seconds(EVENT_TIMER));

        m_broadcast_timer.update();

        m_ptr_rpc_socket->listen(rpc_bind_to_address);

        m_ptr_eh->add(*m_ptr_rpc_socket);
        m_ptr_eh->add(*m_ptr_p2p_socket);


        if (m_blockchain.length() == 0)
            insert_genesis();
        else
        {
            calc_balance();
            load_transaction_cache();

            SignedBlock signed_block;
            m_blockchain.at(m_blockchain.length() - 1, signed_block);

            Block block;
            std::move(signed_block.block_details).get(block);

            calc_sync_info(block);
        }
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
        clear_sync_state(peerid);
        reset_stored_request(peerid);

        if (0 == m_p2p_peers.erase(peerid))
            throw std::runtime_error("p2p peer not found to remove: " + peerid);
    }

    bool find_stored_request(socket::peer_id const& peerid, beltpp::packet& packet)
    {
        auto it = m_stored_requests.find(peerid);
        if (it != m_stored_requests.end())
        {
            BlockchainMessage::detail::assign_packet(packet, it->second.packet);
            return true;
        }

        return false;
    }

    void reset_stored_request(beltpp::isocket::peer_id const& peerid)
    {
        m_stored_requests.erase(peerid);
    }

    void store_request(socket::peer_id const& peerid, beltpp::packet const& packet)
    {
        detail::packet_and_expiry pck;
        BlockchainMessage::detail::assign_packet(pck.packet, packet);
        pck.expiry = PACKET_EXPIRY_STEPS;
        auto res = m_stored_requests.insert(std::make_pair(peerid, std::move(pck)));
        if (false == res.second)
            throw std::runtime_error("only one request is supported at a time");
    }

    bool sync_timeout()
    {
        system_clock::time_point current_time_point = system_clock::now();
        system_clock::time_point previous_time_point = system_clock::from_time_t(sync_time.tm);

        chrono::seconds diff_seconds = chrono::duration_cast<chrono::seconds>(current_time_point - previous_time_point);

        return diff_seconds.count() >= SYNC_FAILURE_TIMEOUT;
    }

    void update_sync_time()
    {
        sync_time.tm = system_clock::to_time_t(system_clock::now());
    }

    void clear_sync_state(beltpp::isocket::peer_id peerid)
    {
        if (peerid == sync_peerid)
        {
            sync_peerid.clear();
            sync_blocks.clear();
            sync_headers.clear();
            sync_responses.clear();
        }
    }

    void new_sync_request()
    {
        // shift next sync
        m_sync_timer.update();

        // clear state
        clear_sync_state(sync_peerid);

        // send new request to all peers
        beltpp::isocket* psk = m_ptr_p2p_socket.get();

        SyncRequest sync_request;

        for (auto& peerid : m_p2p_peers)
        {
            packet stored_packet;
            find_stored_request(peerid, stored_packet);

            if (stored_packet.empty())
            {
                psk->send(peerid, sync_request);
                reset_stored_request(peerid);
                store_request(peerid, sync_request);
            }
        }

        update_sync_time();
    }

    void save(beltpp::on_failure& guard)
    {
        m_state.save();
        m_blockchain.save();
        m_action_log.save();
        m_transaction_pool.save();

        guard.dismiss();

        m_state.commit();
        m_blockchain.commit();
        m_action_log.commit();
        m_transaction_pool.commit();
    }

    void discard()
    {
        m_state.discard();
        m_blockchain.discard();
        m_action_log.discard();
        m_transaction_pool.discard();
    }

    void load_transaction_cache()
    {
        uint64_t block_number = m_blockchain.length();
        uint64_t block_count = TRANSACTION_LIFETIME / BLOCK_MINE_DELAY;

        block_count = block_number < block_count ? block_number : block_count;

        writeln_node("Loading " + std::to_string(block_count) + " blocks to cache");

        while (block_count > 0)
        {
            --block_count;
            --block_number;
            SignedBlock signed_block;
            m_blockchain.at(block_number, signed_block);

            Block block;
            std::move(signed_block.block_details).get(block);

            for (auto& item : block.signed_transactions)
            {
                string key = meshpp::hash(item.to_string());
                m_transaction_cache[key] = system_clock::from_time_t(item.transaction_details.creation.tm);
            }
        }

        writeln_node("done");
    }

    void clean_transaction_cache()
    {
        BlockHeader current_header;
        m_blockchain.header(current_header);

        system_clock::time_point cur_time_point = system_clock::from_time_t(current_header.sign_time.tm);

        auto it = m_transaction_cache.begin();
        while (it != m_transaction_cache.end())
        {
            if (chrono::duration_cast<chrono::seconds>(cur_time_point - it->second).count() > TRANSACTION_LIFETIME)
                it = m_transaction_cache.erase(it);
            else
                ++it;
        }
    }

    uint64_t calc_delta(string const& key, uint64_t const& amount, string const& prev_hash, uint64_t const& cons_const)
    {
        uint64_t dist = meshpp::distance(meshpp::hash(key), prev_hash);
        uint64_t delta = amount * DIST_MAX / ((dist + 1) * cons_const);

        if (delta > DELTA_MAX)
            delta = DELTA_MAX;

        return delta;
    }

    void calc_balance()
    {
        m_balance = m_state.get_balance(m_pv_key.get_public_key().to_string());
        m_miner = coin(m_balance) >= MINE_AMOUNT_THRESHOLD;
    }

    void calc_sync_info(Block const& block)
    {
        own_sync_info.number = m_blockchain.length();
        own_sync_info.authority = m_pv_key.get_public_key().to_string(); // test

        // calculate delta for next block for the case if I will mine it
        if (m_miner)
        {
            string prev_hash = meshpp::hash(block.to_string());
            uint64_t delta = calc_delta(own_sync_info.authority, m_balance.whole, prev_hash, block.header.c_const);

            own_sync_info.c_sum = block.header.c_sum + delta;
            net_sync_info.c_sum = 0;
            net_sync_info.number = 0;
        }
        else
        {
            own_sync_info.c_sum = 0;
            net_sync_info.c_sum = 0;
            net_sync_info.number = 0;
        }
    }

    void insert_genesis()
    {
        if (m_blockchain.length() > 0)
            return;

        BlockHeader block_header;
        block_header.block_number = 0;
        block_header.c_sum = 0;
        block_header.delta = 0;
        block_header.c_const = 1;
        block_header.prev_hash = meshpp::hash("Ice Age");
        block_header.sign_time.tm = 0;

        Block block;
        block.header = block_header;

        Reward reward;
        reward.amount = coin(100, 0).to_Coin();

        meshpp::random_seed node_rs("NODE");
        meshpp::private_key node_pv = node_rs.get_private_key(0);
        reward.to = node_pv.get_public_key().to_string();
        block.rewards.push_back(std::move(reward));

        meshpp::random_seed armen_rs("ARMEN");
        meshpp::private_key armen_pv = armen_rs.get_private_key(0);
        reward.to = armen_pv.get_public_key().to_string();
        block.rewards.push_back(std::move(reward));

        meshpp::random_seed tigran_rs("TIGRAN");
        meshpp::private_key tigran_pv = tigran_rs.get_private_key(0);
        reward.to = tigran_pv.get_public_key().to_string();
        block.rewards.push_back(std::move(reward));

        meshpp::random_seed gagik_rs("GAGIK");
        meshpp::private_key gagik_pv = gagik_rs.get_private_key(0);
        reward.to = gagik_pv.get_public_key().to_string();
        block.rewards.push_back(std::move(reward));

        meshpp::random_seed sona_rs("SONA");
        meshpp::private_key sona_pv = sona_rs.get_private_key(0);
        reward.to = sona_pv.get_public_key().to_string();
        block.rewards.push_back(std::move(reward));

        meshpp::random_seed rs("GENESIS");
        meshpp::private_key pv_key = rs.get_private_key(0);
        meshpp::signature sgn = pv_key.sign(block.to_string());

        SignedBlock signed_block;
        signed_block.signature = sgn.base58;
        signed_block.authority = sgn.pb_key.to_string();
        signed_block.block_details = block;

        beltpp::on_failure guard([&] { discard(); });

        // apply rewards to state and action_log
        for (auto& item : block.rewards)
            m_state.increase_balance(item.to, item.amount);

        // insert to blockchain and action_log
        m_blockchain.insert(signed_block);
        m_action_log.log_block(signed_block);

        save(guard);
        calc_balance();
        calc_sync_info(block);
    }

    std::vector<beltpp::isocket::peer_id> do_step()
    {
        vector<beltpp::isocket::peer_id> result;

        for (auto& key_value : m_stored_requests)
        {
            if (0 == key_value.second.expiry)
                result.push_back(key_value.first);

            --key_value.second.expiry;
        }
        return result;
    }

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

    publiqpp::blockchain m_blockchain;
    publiqpp::action_log m_action_log;
    publiqpp::storage m_storage;
    publiqpp::transaction_pool m_transaction_pool;
    publiqpp::state m_state;

    unordered_set<beltpp::isocket::peer_id> m_p2p_peers;
    unordered_map<beltpp::isocket::peer_id, packet_and_expiry> m_stored_requests;
    unordered_map<string, system_clock::time_point> m_transaction_cache;

    meshpp::private_key m_pv_key;

    bool m_miner;
    Coin m_balance;
    
    SyncInfo own_sync_info;
    SyncInfo net_sync_info;
    BlockchainMessage::ctime sync_time;
    beltpp::isocket::peer_id sync_peerid;
    vector<SignedBlock> sync_blocks;
    vector<BlockHeader> sync_headers;
    vector<std::pair<beltpp::isocket::peer_id, SyncResponse>> sync_responses;
};

}
}
