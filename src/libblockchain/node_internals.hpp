#pragma once

#include "common.hpp"
#include "http.hpp"

#include "state.hpp"
#include "storage.hpp"
#include "action_log.hpp"
#include "blockchain.hpp"
#include "session_manager.hpp"

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

#include <map>
#include <chrono>
#include <memory>

using namespace libblockchain;
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
    using rpc_sf = beltpp::socket_family_t<&http::message_list_load>;

namespace detail
{

class packet_and_expiry
{
public:
    beltpp::packet packet;
    size_t expiry;
};

class stat_counter  //TODO move to better place
{
public:
    void init(string new_hash)
    {
        if (block_hash != new_hash)
        {
            ussage_map.clear();
            block_hash = new_hash;
        }
    }

    void update(string node_name, bool success)
    {
        if(success)
            ussage_map[node_name].first++;
        else
            ussage_map[node_name].second++;
    }

    bool get_stat_info(StatInfo& stat_info)
    {
        stat_info.items.clear();
        stat_info.hash = block_hash;

        for (auto& item : ussage_map)
        {
            StatItem stat_item;
            stat_item.node = item.first;
            stat_item.passed = item.second.first;
            stat_item.failed = item.second.second;

            stat_info.items.push_back(stat_item);
        }

        return !ussage_map.empty();
    }

private:
    string block_hash;
    map<string, pair<uint64_t, uint64_t>> ussage_map;
};

class node_internals
{
public:
    node_internals(
        ip_address const& rpc_bind_to_address,
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
        NodeType& n_type,
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
        , m_rpc_bind_to_address(rpc_bind_to_address)
        , m_blockchain(fs_blockchain)
        , m_action_log(fs_action_log, log_enabled)
        , m_storage(fs_storage)
        , m_transaction_pool(fs_transaction_pool)
        , m_state(fs_state)
        , m_node_type(n_type)
        , m_pv_key(pv_key)
        , m_pb_key(pv_key.get_public_key())
    {
        m_sync_timer.set(chrono::seconds(SYNC_TIMER));
        m_check_timer.set(chrono::seconds(CHECK_TIMER));
        m_broadcast_timer.set(chrono::seconds(BROADCAST_TIMER));
        m_cache_cleanup_timer.set(chrono::seconds(CACHE_CLEANUP_TIMER));
        m_summary_report_timer.set(chrono::seconds(SUMMARY_REPORT_TIMER));
        m_ptr_eh->set_timer(chrono::seconds(EVENT_TIMER));

        m_broadcast_timer.update();

        if (false == rpc_bind_to_address.local.empty())
            m_ptr_rpc_socket->listen(rpc_bind_to_address);

        m_ptr_eh->add(*m_ptr_rpc_socket);
        m_ptr_eh->add(*m_ptr_p2p_socket);

        if (m_blockchain.length() == 0)
            insert_genesis();

        calc_balance();
        load_transaction_cache();

        SignedBlock signed_block;
        m_blockchain.at(m_blockchain.length() - 1, signed_block);

        calc_sync_info(signed_block.block_details);

        if(m_node_type == NodeType::storage)
            m_stat_counter.init(meshpp::hash(signed_block.block_details.to_string()));
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
        writeln_node("Loading recent blocks to cache");

        std::chrono::system_clock::time_point time_signed_head;

        uint64_t block_count = m_blockchain.length();
        for (uint64_t block_index = block_count - 1;
             block_index < block_count;
             --block_index)
        {
            SignedBlock signed_block;
            m_blockchain.at(block_index, signed_block);

            Block& block = signed_block.block_details;

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
    }

    void clean_transaction_cache()
    {
        BlockHeader current_header;
        m_blockchain.last_header(current_header);

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
        m_balance = m_state.get_balance(m_pb_key.to_string());
        m_miner = m_node_type == NodeType::miner &&
                  coin(m_balance) >= MINE_AMOUNT_THRESHOLD;
    }

    void calc_sync_info(Block const& block)
    {
        own_sync_info.number = m_blockchain.length();

        // calculate delta for next block for the case if I will mine it
        if (m_miner)
        {
            string prev_hash = meshpp::hash(block.to_string());
            uint64_t delta = calc_delta(m_pb_key.to_string(), m_balance.whole, prev_hash, block.header.c_const);

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

        SignedBlock signed_block;

        signed_block.from_string(R"genesis({"rtt":6,"block_details":{"rtt":5,"header":{"rtt":4,"block_number":0,"delta":0,"c_sum":0,"c_const":1,"prev_hash":"2e3WkhHavShVgKyuvX9HNEF185zSqCoG73UHCR9cyPcr","time_signed":"2018-10-18 13:13:59"},"rewards":[{"rtt":10,"amount":{"rtt":0,"whole":100,"fraction":0},"to":"PBQ7Ta31VaxCB9VfDRvYYosKYpzxXNgVH46UkM9i4FhzNg4JEU3YJ"},{"rtt":10,"amount":{"rtt":0,"whole":100,"fraction":0},"to":"PBQ76Zv5QceNSLibecnMGEKbKo3dVFV6HRuDSuX59mJewJxHPhLwu"},{"rtt":10,"amount":{"rtt":0,"whole":100,"fraction":0},"to":"PBQ7JEFjtQNjyzwnThepF2jJtCe7cCpUFEaxGdUnN2W9wPP5Nh92G"},{"rtt":10,"amount":{"rtt":0,"whole":100,"fraction":0},"to":"PBQ8f5Z8SKVrYFES1KLHtCYMx276a5NTgZX6baahzTqkzfnB4Pidk"},{"rtt":10,"amount":{"rtt":0,"whole":100,"fraction":0},"to":"PBQ8MiwBdYzSj38etLYLES4FSuKJnLPkXAJv4MyrLW7YJNiPbh4z6"},{"rtt":10,"amount":{"rtt":0,"whole":10,"fraction":75000000},"to":"PBQ4wgwQW8HuzVyc91L5Uwiqe7pRhsfcHCYH9PaNhojvm4hzjY1K3"},{"rtt":10,"amount":{"rtt":0,"whole":1516,"fraction":33716818},"to":"PBQ4wkh5G7p46dVfy6J1vEfb1UnF1T9c4xm5vgc5Qhjfm9aJTTWGW"},{"rtt":10,"amount":{"rtt":0,"whole":954,"fraction":54545455},"to":"PBQ4xB8cEA2RgyTdnEAF1TWzZ7og1pk6LsSLsRRAgMspanxKe4MF4"},{"rtt":10,"amount":{"rtt":0,"whole":800,"fraction":0},"to":"PBQ4yFkY1Yz7NHpqCDmVEkLi4h2JebdaSB4YcpGjgDhXv5NDnMR5o"},{"rtt":10,"amount":{"rtt":0,"whole":4139,"fraction":76000000},"to":"PBQ4yhf5HdnJa5RmcUT9m2yQfQEQriDNRtv74cqfmhVwWypz8VcBi"},{"rtt":10,"amount":{"rtt":0,"whole":1768840,"fraction":85629630},"to":"PBQ517nh5NLYMRCwXJYQSBjyn5aPTzCY53HMzkRphcSryfycmb8KP"},{"rtt":10,"amount":{"rtt":0,"whole":1600,"fraction":0},"to":"PBQ52CMwubdJ9SDd1QuvUR5CRYX1BDbPRe6vUq3FBUjcrJvnH2meX"},{"rtt":10,"amount":{"rtt":0,"whole":1848,"fraction":1995000},"to":"PBQ52NqaAfY6yzv6MnsbXi8nSsyU8ii83Fr7UHCUMujK7wEpQDbqU"},{"rtt":10,"amount":{"rtt":0,"whole":1600,"fraction":0},"to":"PBQ538sm5RCinRqC2TjRbu66Dd4mg15EC53Hwd43dfYFnn4jY9TSH"},{"rtt":10,"amount":{"rtt":0,"whole":5908,"fraction":26545217},"to":"PBQ53TasznohxcrTAsnJj9qVMWkGP8LGd9x85xMDT1otSqqRxxZHD"},{"rtt":10,"amount":{"rtt":0,"whole":1600,"fraction":0},"to":"PBQ53UyRBYBpYoFNyX4bfSHmBuNnE75PdsLn6AiLeA2TffvVyDxTY"},{"rtt":10,"amount":{"rtt":0,"whole":138,"fraction":60000000},"to":"PBQ53Y3KPZnpXwQQuebKoCAZjHZoVzW6F93VJz8p6GCStEwBTNQgj"},{"rtt":10,"amount":{"rtt":0,"whole":9503,"fraction":33369999},"to":"PBQ54WyQxRkZdntFtFnpdtQgYJoBbckcZzJoGEnwzj7EYFHsB1hpS"},{"rtt":10,"amount":{"rtt":0,"whole":3029,"fraction":14500000},"to":"PBQ54fsvCrELPgRVPHBj34ySBzVTSek1yYBQGeAW51PxgkeyYLmjV"},{"rtt":10,"amount":{"rtt":0,"whole":5250,"fraction":0},"to":"PBQ551Lho1u9PzYr6Cz3vitD4sRKGeDmSvEQxAvowbcNvx2z66d2p"},{"rtt":10,"amount":{"rtt":0,"whole":800,"fraction":0},"to":"PBQ551VfU2Du4yhvPDRzXf9Y4r6ZXcqc8VkyojdAUHXoz1tdhRGP6"},{"rtt":10,"amount":{"rtt":0,"whole":47,"fraction":72727273},"to":"PBQ56kA5QwGPXhPVan2YG5hYZWEpddgHmcVjmo7NCfXrPDJPCrAL1"},{"rtt":10,"amount":{"rtt":0,"whole":600,"fraction":0},"to":"PBQ58oKeA4dFHM7o9bSnCRBwXHE9EMUNR9fyQTSMzNnQ5EFHsM2HE"},{"rtt":10,"amount":{"rtt":0,"whole":11458,"fraction":33333333},"to":"PBQ58wet2pue42J9vGiuLdwGhC5M75Xk5B5b3rsHmDDCcYyYX5ewx"},{"rtt":10,"amount":{"rtt":0,"whole":350,"fraction":0},"to":"PBQ59uXLyZErDHCddbwnCmkLh9tqhrfqqGKVDCeCupep92wEBj76o"},{"rtt":10,"amount":{"rtt":0,"whole":769,"fraction":36363636},"to":"PBQ5ABXv3MnRZNa9bJTScAJihgmugJwHv3hEJEAKttQKjj2shu2jX"},{"rtt":10,"amount":{"rtt":0,"whole":3064,"fraction":79670000},"to":"PBQ5C7x56BrGvCRiPzrQs5JAJdmfNzVVsTGCbh584sK1wTBCcHX1q"},{"rtt":10,"amount":{"rtt":0,"whole":20790,"fraction":0},"to":"PBQ5Frr7jB8TAHtWCwR6vkHcXT1NmmnhBrN2d6VTEsuw5dswZX44a"},{"rtt":10,"amount":{"rtt":0,"whole":1750,"fraction":0},"to":"PBQ5Ghcu4QtJARSCbLVzFHqqeTy7h48frc6fhxBDSurYwRf6sffwE"},{"rtt":10,"amount":{"rtt":0,"whole":1458,"fraction":33333333},"to":"PBQ5GibPyJyvsqohHowxxEzJp32t6K98kMn49NUjiU5RNby6VcSk8"},{"rtt":10,"amount":{"rtt":0,"whole":800,"fraction":0},"to":"PBQ5Hqxra9qr8Y5MKkQm6uTLKxy2NnahtdJpPSavh8dGaAFq6Sihk"},{"rtt":10,"amount":{"rtt":0,"whole":1600,"fraction":0},"to":"PBQ5JBKm1ZfGCQ1tz4yjNP7GTeFmLgnyH67Poi4eTsiUB6MVNV5Ku"},{"rtt":10,"amount":{"rtt":0,"whole":955,"fraction":50000000},"to":"PBQ5JTrMBZ9U6YM43e4VK4nrKBRKAZ2cJqRauUAofXVHa8mQAte6Z"},{"rtt":10,"amount":{"rtt":0,"whole":5250,"fraction":0},"to":"PBQ5KTmDPoan1Kt8wVJ7Lj5QjWafFa2gGSPDNtNqeVfspHGrA7CBu"},{"rtt":10,"amount":{"rtt":0,"whole":1000,"fraction":0},"to":"PBQ5LNmKywtwhYSy7gAbTyyacMUm49GHiTCCC95329rmXQHqnBDYe"},{"rtt":10,"amount":{"rtt":0,"whole":10000,"fraction":0},"to":"PBQ5LYNrLCngGjRVuWhVq4eR9DYdgtqqxUj1vzRK9N9sXv5F3ufTH"},{"rtt":10,"amount":{"rtt":0,"whole":1871,"fraction":69250000},"to":"PBQ5MUwVdzhPfaWK6YvU48oM1UxG4PxNfH6oyrKsri85hZfKoegHG"},{"rtt":10,"amount":{"rtt":0,"whole":875,"fraction":0},"to":"PBQ5NbY2nUZHbZ1suUMzDapXtXNsx5LVvVVFCwhm2QcwQZ3zXZp8M"},{"rtt":10,"amount":{"rtt":0,"whole":5586,"fraction":0},"to":"PBQ5Q3GXffjZM4LekVxg612TpxXJrX7JoMWDAD5mfJZxXrVKkhT2P"},{"rtt":10,"amount":{"rtt":0,"whole":104827,"fraction":3140000},"to":"PBQ5Q8nLbeGkphGLypZjompKJiyacpD1ZrVctDBL6HZqi8X1mReZA"},{"rtt":10,"amount":{"rtt":0,"whole":957,"fraction":89226522},"to":"PBQ5TNit9oDf8uhPruJzviUHKFPbB4HjscK6XwtyRLM51T6xwJJGa"},{"rtt":10,"amount":{"rtt":0,"whole":73,"fraction":24800000},"to":"PBQ5U5smUFhVS382t2VS3sKSKmf8Ui3YprEoi3BT8SVNTBLYEEsu8"},{"rtt":10,"amount":{"rtt":0,"whole":140,"fraction":45714286},"to":"PBQ5UvoqEhpnMkWijM3peTyAN66bQpsNrEgETjQAf9u9fPvzLrjV8"},{"rtt":10,"amount":{"rtt":0,"whole":241,"fraction":66765000},"to":"PBQ5UysUAwo9uVbSCzEqSDBwyErVrx2eogUAaGD7QUdHXG1DnLC2o"},{"rtt":10,"amount":{"rtt":0,"whole":308,"fraction":70000000},"to":"PBQ5XpvxpMHt7oV5BmRchFLKP6YVzr4HufjEXVzgXsX6uwEo3HCyB"},{"rtt":10,"amount":{"rtt":0,"whole":188,"fraction":57142857},"to":"PBQ5Xv5TSqm9SWGqKusjYH7328bH9S41Z9awnwF2exGYSvdjjkXPW"},{"rtt":10,"amount":{"rtt":0,"whole":1600,"fraction":0},"to":"PBQ5YCwPnD8Sd9ozCV1Pz7S9CYdPkM4NLJSAGSpexN62RNgeL2Y1P"},{"rtt":10,"amount":{"rtt":0,"whole":0,"fraction":0},"to":"PBQ5YE4tHiPf31opMf7UMbvi1cWuh4PFYVzKBgmUiapM8qD5EytKN"},{"rtt":10,"amount":{"rtt":0,"whole":19506,"fraction":25425000},"to":"PBQ5Yb4HFLiMW4ypG3etkDyvZnG5RMwCm29Y9MfD2ehUgvEZ6WHGd"},{"rtt":10,"amount":{"rtt":0,"whole":12352,"fraction":51500000},"to":"PBQ5ZkmwyLd3zAVkiix6bKnyvredvEwWB6ya1xuJgzPY3iXXcLin5"},{"rtt":10,"amount":{"rtt":0,"whole":315,"fraction":0},"to":"PBQ5cb6zRnUrQsLJbkNPb1zbD2MRQak7WwGA8srdu45MW3HsUq7oE"},{"rtt":10,"amount":{"rtt":0,"whole":3571,"fraction":42857143},"to":"PBQ5dqTxmJYUXYdH5Kvcx7rL3CGLDPYH368rtgezim6qS8RwGHfBD"},{"rtt":10,"amount":{"rtt":0,"whole":5714,"fraction":28571429},"to":"PBQ5eFtFtnfPMhisGkU7M1JfPQLv7iRpvX4kZh7zDHodKV2GYWvMs"},{"rtt":10,"amount":{"rtt":0,"whole":1050,"fraction":0},"to":"PBQ5fBPYawbhLqGbaky5t8dGRTCqLcWowL5PGY5cjnu9LaQPBaWBs"},{"rtt":10,"amount":{"rtt":0,"whole":2006,"fraction":99173500},"to":"PBQ5hhqUp4SkfuuNApTwxYUdKkefYyWtHAzQpu22YejAgngyJ4BGV"},)genesis"
                                 R"genesis({"rtt":10,"amount":{"rtt":0,"whole":1650,"fraction":48000000},"to":"PBQ5i3NyoNkwbQwDQE7kYv9vN2FBKW9MvEaVuKQLijwCh1LKDeyBf"},{"rtt":10,"amount":{"rtt":0,"whole":1936,"fraction":34090000},"to":"PBQ5m3i4ySGSibPvUyWdJaSyz8Tgtpr9Fq2gSNJvWvCYycujPEBmj"},{"rtt":10,"amount":{"rtt":0,"whole":5714,"fraction":28571429},"to":"PBQ5nquogmr4yKcb7o6TwG3SYnUF4wptP6x2gCmW93WEtkrtdvkHN"},{"rtt":10,"amount":{"rtt":0,"whole":595,"fraction":20120000},"to":"PBQ5p5vKSpQBrJi1BGtt4Rcc68KEGBav1P8vVoNkdiiL7BUYP4vd5"},{"rtt":10,"amount":{"rtt":0,"whole":229,"fraction":16666667},"to":"PBQ5phwave4q1NqYmh3htjecLAtBMCbEZpo6jw6Rg4dyuSMbT85td"},{"rtt":10,"amount":{"rtt":0,"whole":12863,"fraction":55000000},"to":"PBQ5pqfLwSJF9nriSrjucLX447aw9kAiW7b83hWeB98g98q3jAiiN"},{"rtt":10,"amount":{"rtt":0,"whole":382,"fraction":73340000},"to":"PBQ5qDc9E2fSvxawnDcJdp7NJoYo52sXm7JD1nvLNi2xjTyyv5QWR"},{"rtt":10,"amount":{"rtt":0,"whole":800,"fraction":0},"to":"PBQ5rAGquGmn53TXMXq8RBQyx5dSmhsctBLjtBMgYd9hZvBACY4Zn"},{"rtt":10,"amount":{"rtt":0,"whole":1600,"fraction":0},"to":"PBQ5rW8Sqt3pJr4gWQQXqFB8HBzJnQdnW7uNsVNcYhtkQFQT3Vyih"},{"rtt":10,"amount":{"rtt":0,"whole":82807,"fraction":98000000},"to":"PBQ5rkjzTra43hx1M5kGrfVP4TSg2hyVU1pmSoBKyBx142bsfX7FE"},{"rtt":10,"amount":{"rtt":0,"whole":5250,"fraction":0},"to":"PBQ5rpo2v9cCjfTX7oMLGFxXAMNrWpbUsyauLmcso3RLgFQApCHAe"},{"rtt":10,"amount":{"rtt":0,"whole":50080,"fraction":0},"to":"PBQ5sGCE2mdBmejg98dRQQhRrk5AtF4EeYytG2PjYPaeErASPwiZJ"},{"rtt":10,"amount":{"rtt":0,"whole":875,"fraction":0},"to":"PBQ5vMreKfCM4b1QBdkaEjDdjCqDjk3LoMTZTRpmam57R6ZHeyMSH"},{"rtt":10,"amount":{"rtt":0,"whole":0,"fraction":0},"to":"PBQ5wBkAp4Xp11RWoJghfQegY84A1FB7DdgFShwi88EPUrd8U1d5S"},{"rtt":10,"amount":{"rtt":0,"whole":2150,"fraction":0},"to":"PBQ5wMUaqnaD7gFpVtsa3HKjT2NqNTsJ2HDkSJvNQFYzDNnQwood3"},{"rtt":10,"amount":{"rtt":0,"whole":168089,"fraction":26005238},"to":"PBQ5x9oBt4fhNUbN2tVXkvRnudq9nAYoEyeZWhfHS1rVQ6HkhwpZc"},{"rtt":10,"amount":{"rtt":0,"whole":610,"fraction":90909091},"to":"PBQ5y72FrSWU7iGKexci3MWN5sqvKKZBhy55QVW47DDCTfZDrJFwj"},{"rtt":10,"amount":{"rtt":0,"whole":506,"fraction":41320000},"to":"PBQ5yRW9nTs21aymuc1Z1CW4XzjQsvSMZWgp52yL1tumDWQF7Dksp"},{"rtt":10,"amount":{"rtt":0,"whole":75,"fraction":83333333},"to":"PBQ5zSTAfn2wYmyP8CyUfFG3uLMFM3zYSNBcz97nj9MJ1Vh1uoikY"},{"rtt":10,"amount":{"rtt":0,"whole":1200,"fraction":0},"to":"PBQ5zVobj1Bj4YGBU3GG93wEMcDsutHKtWVdk5u6N5iX2wzYPTjy1"},{"rtt":10,"amount":{"rtt":0,"whole":537,"fraction":50000000},"to":"PBQ5zg5iz3nq5BkP8xhzjNicPrzJSfyj6jHebtjaXWXzJiAyPZEKZ"},{"rtt":10,"amount":{"rtt":0,"whole":105,"fraction":0},"to":"PBQ61N5Xza3Rx8gyChEResjCarjipRT4U8LUKpivVBKv1WGqsbxU9"},{"rtt":10,"amount":{"rtt":0,"whole":400,"fraction":0},"to":"PBQ62DATP7XmGKnbg7TTxAwQpMEkzQ52ye8i6h3MQPooC2ggfb99t"},{"rtt":10,"amount":{"rtt":0,"whole":1341910,"fraction":50000000},"to":"PBQ62faC8BCG66NssjUzCcFPizvBELFzVXj9aBvwLcjBdeSr9oaK9"},{"rtt":10,"amount":{"rtt":0,"whole":8410,"fraction":18205999},"to":"PBQ6325naDUxBNWLydRKMyREuZuTHbNpPR1URQfWKbiHhjb9fJBpa"},{"rtt":10,"amount":{"rtt":0,"whole":4545,"fraction":45454545},"to":"PBQ66gzSn4P8BHwMzQyFToUdHeWhVUPi1Bdf2VuB3oGPhXejGVhRX"},{"rtt":10,"amount":{"rtt":0,"whole":1540,"fraction":0},"to":"PBQ6FULRRPZeK7mcScqYk2Rzf2ay9pehuzL7PToFaJz99GEdUv9PE"},{"rtt":10,"amount":{"rtt":0,"whole":846,"fraction":84600000},"to":"PBQ6GcjBmkhrLPfWr6dk2mCJeAdyr5Z5w61XsmmQTo9iEgWeUdcMv"},{"rtt":10,"amount":{"rtt":0,"whole":5000740,"fraction":74074074},"to":"PBQ6JFt4ABPjGvWQ41GPkhbnss8KqHEYUJoV8JjyrwRXe6KawBZLz"},{"rtt":10,"amount":{"rtt":0,"whole":0,"fraction":1000000},"to":"PBQ6JWuujuLqfYtrzBVTvPmQdSLQ7aMxS9HEbK66rShbiEv6XNVKH"},{"rtt":10,"amount":{"rtt":0,"whole":1071,"fraction":0},"to":"PBQ6JooaHzduAdTYarLMP4n94vrzci5HTYEbFw7P9sZmmLXqKpVWL"},{"rtt":10,"amount":{"rtt":0,"whole":121,"fraction":15384615},"to":"PBQ6N4BCDJN1TXF5Pzjku83ubDX7Wy9mgaNa8GNzbsLLwnxVMrCGk"},{"rtt":10,"amount":{"rtt":0,"whole":525,"fraction":0},"to":"PBQ6Rkbqy3NeBtzQCjjqpZajF1aKv8anpEvwjMQChWHPiFB84wEoF"},{"rtt":10,"amount":{"rtt":0,"whole":352552,"fraction":99000000},"to":"PBQ6SH68cD3tTCEKVRY2okTAHuHhq2ms34xndiHarbTbBD62gzqXu"},{"rtt":10,"amount":{"rtt":0,"whole":2916,"fraction":66666667},"to":"PBQ6U3fYPaqDrqq7aNonh8wSN4zsYDFNjAPi18H8haGdrnvV1XVns"},{"rtt":10,"amount":{"rtt":0,"whole":115,"fraction":52835000},"to":"PBQ6UaEWLhVLrHkdbi1oK3kWpp6udvmkfAhTccj9GaZhfmMH7bGmZ"},{"rtt":10,"amount":{"rtt":0,"whole":6300,"fraction":0},"to":"PBQ6V4za7MiJe3M7fB8W14HkYPqqcSAoXq5X6frL9AvaEuFHUZQPG"},{"rtt":10,"amount":{"rtt":0,"whole":0,"fraction":0},"to":"PBQ6XU8UQd2Lqc4Wv7hUxNpoqpfYb2ZyD2eXvfNaY9iAybBMgAD1U"},{"rtt":10,"amount":{"rtt":0,"whole":800,"fraction":0},"to":"PBQ6Xwc86wA4ibfxc23x9BiY9kJexT7cFUnxJ2NFGJYcbPwtXydi4"},{"rtt":10,"amount":{"rtt":0,"whole":708,"fraction":18825000},"to":"PBQ6Y18JF9aWeLbCeMZriHczpGSxhgjPN7HgNwCT2K1ijDjTqLFLc"},{"rtt":10,"amount":{"rtt":0,"whole":78,"fraction":75000000},"to":"PBQ6YNPHoynTa4ULd3w47LaeU58xrf1NTK7jiC8uiyupCigwC8vhU"},{"rtt":10,"amount":{"rtt":0,"whole":1333,"fraction":70000000},"to":"PBQ6Yb4pbxU2hmkHf2GfRTJ9m1RCW9faxnx58idwGmdsEUdPsWiEN"},{"rtt":10,"amount":{"rtt":0,"whole":789,"fraction":28500000},"to":"PBQ6ZeZc7oJeZwUoB1Jrk7zHcZ8vw4UQbvvMAg4hhqMnc5hMeCTQx"},{"rtt":10,"amount":{"rtt":0,"whole":800,"fraction":0},"to":"PBQ6abXB6kQPNaH76JRKGFc137fwtPHJ5LRFon2QuARDJVXqPWFYX"},{"rtt":10,"amount":{"rtt":0,"whole":5271,"fraction":0},"to":"PBQ6dXQNUygsjYu6vwmwVXeufbfAnPgiy3hsY6xrEgkLYqVadWVbZ"},{"rtt":10,"amount":{"rtt":0,"whole":800,"fraction":0},"to":"PBQ6deUr4iCVWYB4dxAmRYjD9z3jAZTzz2FP4DaeBwc7LmRLPFqgr"},{"rtt":10,"amount":{"rtt":0,"whole":102,"fraction":99240000},"to":"PBQ6dtrFsyiuesLN2vfB1X9xgKCPWPdPyuic8v8kaJpTgw4JJjFkT"},{"rtt":10,"amount":{"rtt":0,"whole":954,"fraction":54545455},"to":"PBQ6eK4m6JDXJ3NzXkxKHoePVrTF83yK39FVJChy43dLKbxJu6XAj"},{"rtt":10,"amount":{"rtt":0,"whole":1050,"fraction":0},"to":"PBQ6f269NsiAqJeiYjEbaA7y6QTdEFbrjX9xjrVXMJPKwGWK1YU9p"},{"rtt":10,"amount":{"rtt":0,"whole":0,"fraction":0},"to":"PBQ6k7NrLryUW7pgpzyw43YexB12Djd5F7E5qJ7h6PhqW3GfeXp44"},{"rtt":10,"amount":{"rtt":0,"whole":10447,"fraction":50000000},"to":"PBQ6mV5S8d5E2hPcHd9bWLicGgA51NRBGiaHGetuPbPdbifnjWBW2"},{"rtt":10,"amount":{"rtt":0,"whole":36,"fraction":66666667},"to":"PBQ6nZgk34r3GJuzVMj9ehfUGs9raSTaXHp8pgisYN88MWuBKCK8j"},{"rtt":10,"amount":{"rtt":0,"whole":0,"fraction":0},"to":"PBQ6o8Bg9HZyx7q6hbz4Zjk5nhsZYhJxsVcLxewL197g3tJSCR77k"},{"rtt":10,"amount":{"rtt":0,"whole":1600,"fraction":0},"to":"PBQ6p841qQ1sSDhdtuM9a5fNnHi7idRuJhxaRb8iHaCnDgEC8iFZL"},{"rtt":10,"amount":{"rtt":0,"whole":800,"fraction":0},"to":"PBQ6pSkKR37nn6PS86xQMZ2VL5YWYodvuNHamVVCUXBozQxPotznw"},{"rtt":10,"amount":{"rtt":0,"whole":0,"fraction":0},"to":"PBQ6qA9QuFyp5BFcQD13fqv5V8eVfQL5z1rRoGDiddqFpTaNVbycG"},{"rtt":10,"amount":{"rtt":0,"whole":1050,"fraction":0},"to":"PBQ6qHhFKk8ZQhkDWZkSsqBRiW1oUqNWAefwbHeKBVUksCPeT3jj4"},)genesis"
                                 R"genesis({"rtt":10,"amount":{"rtt":0,"whole":956712,"fraction":0},"to":"PBQ6qWtpnjKKg6RiD47BTX2i7ZU2gAbSNR1BhSonFdVM4t3HnaNE3"},{"rtt":10,"amount":{"rtt":0,"whole":154,"fraction":41176471},"to":"PBQ6qhHQiZVtVQwg31wxh6Zzu9dUKMT2R9jwrf6DYbLP6gGZs3SGV"},{"rtt":10,"amount":{"rtt":0,"whole":5880,"fraction":0},"to":"PBQ6qwRHMjnq4SSzd8fbJB5RHiySsn61c65crywfjRG9qFfFPeC1c"},{"rtt":10,"amount":{"rtt":0,"whole":7000,"fraction":0},"to":"PBQ6rKfMfKp4wvWmXVLDSgu8xwfUipzjEy8j9cuN5sFCZmDRYDsdP"},{"rtt":10,"amount":{"rtt":0,"whole":735,"fraction":0},"to":"PBQ6sAuBVQ41a7AktpHGzgcpmcaravhVeMvkiGhjp6F6AAQZUJ3H3"},{"rtt":10,"amount":{"rtt":0,"whole":566,"fraction":71439999},"to":"PBQ6sSuW8AvPyPrQHuZuyEFY8wazv6jiH7ahDCk5S3DEiMi21UzX7"},{"rtt":10,"amount":{"rtt":0,"whole":88602,"fraction":72493333},"to":"PBQ6tst1iNcfW9gs5sNgRuaeECkCLmT7FnDGorAXvnFxKfMa4Uc5K"},{"rtt":10,"amount":{"rtt":0,"whole":5080,"fraction":57142856},"to":"PBQ6uWh8zm3WtSw4GcPsNne8MJVGPXajCgchFFRhu6MeDGLeEfT65"},{"rtt":10,"amount":{"rtt":0,"whole":66024,"fraction":0},"to":"PBQ6v8Mbn7WeLWpCiVR7KWa1WU9rJdrKf7YKMHoyx2nvVWxLSyxXg"},{"rtt":10,"amount":{"rtt":0,"whole":499,"fraction":99979167},"to":"PBQ6vGz35mfULFe9CN8A58wCm9aKZhyBAaCxxqdLbD5b4m5xeSmpd"},{"rtt":10,"amount":{"rtt":0,"whole":491,"fraction":83718182},"to":"PBQ6yDp8NVJevUtwZ7j47f3xU9M2vHybpU5w9NVNPZjwiQuW652MH"},{"rtt":10,"amount":{"rtt":0,"whole":196,"fraction":0},"to":"PBQ71Ea6J6fW5CrXFYp7DZKkuAhju9emmF8ukeSqQtXDsnoyMQack"},{"rtt":10,"amount":{"rtt":0,"whole":2848,"fraction":35656000},"to":"PBQ72e6EveE3Cv4oK9ARextNhrygCcoTYJYV4eyXHCbs638hP4S1T"},{"rtt":10,"amount":{"rtt":0,"whole":110,"fraction":0},"to":"PBQ72pnqHC2q9XzG1k6pQEiixUsVoQWCxpA8BCq8ja3TMDepTpRqQ"},{"rtt":10,"amount":{"rtt":0,"whole":280,"fraction":5075000},"to":"PBQ75fjnV5hc95jGTsqzqxj4EzhasMoiBJiwyxZUne4mL7rt7WzVw"},{"rtt":10,"amount":{"rtt":0,"whole":36646,"fraction":70835000},"to":"PBQ76TQQpqMmYvrBnKnT4i2NoM9zMWKDdA2iEoxnUR7jtnW3KsL2o"},{"rtt":10,"amount":{"rtt":0,"whole":477,"fraction":27272727},"to":"PBQ782aqdSEun7wDYh83ToY9rxbdEPWhLE4V7GAGJpoiTtgWa4uGT"},{"rtt":10,"amount":{"rtt":0,"whole":4200,"fraction":0},"to":"PBQ783LAKiNKHBJ5Tykf2VT9LKxVbiGGTYTyjWAhGKkFCALYk5hNC"},{"rtt":10,"amount":{"rtt":0,"whole":12106,"fraction":50000000},"to":"PBQ78pCM2zFP5FrYhuhRwRwLJFETNzmy7dUKPWLrq2WSonhrHEV8H"},{"rtt":10,"amount":{"rtt":0,"whole":306,"fraction":91500000},"to":"PBQ78rCjjm6wVBhVHJiKn7t9wEyHF6vuFCimGTuZkC4dDdmdj9Wg7"},{"rtt":10,"amount":{"rtt":0,"whole":4565,"fraction":21739129},"to":"PBQ79HMVZztAfuJs7SgYkvhv524T8rFCtHcEBiqtrbALZMo8p68Nt"},{"rtt":10,"amount":{"rtt":0,"whole":800,"fraction":0},"to":"PBQ7Bkgvxqk6a2DHoYgZ7vFn7LaAyhSgMtbPtKW4vT4LLUU1p4oYj"},{"rtt":10,"amount":{"rtt":0,"whole":113427,"fraction":3703704},"to":"PBQ7BzrHsKQNwFHxRG1JiiTvpN7wbYjKytsgQpCsejNyGKFrXGxWF"},{"rtt":10,"amount":{"rtt":0,"whole":668,"fraction":18181818},"to":"PBQ7C5w5ZjoLdjFX1RLduXtrWP2de6w62BQbmd2XMSFEaYiMbNNQr"},{"rtt":10,"amount":{"rtt":0,"whole":10032,"fraction":27272727},"to":"PBQ7EiTxbfAf8EGXdh2ecoDgcDZq6mhJjDhyES8uCc3bZKbzrPkvy"},{"rtt":10,"amount":{"rtt":0,"whole":86473,"fraction":70000000},"to":"PBQ7Eprf1ezvf4WUC3Qq6aRzncXXkd9WgKAJYKkDXNsK2bSz1ispi"},{"rtt":10,"amount":{"rtt":0,"whole":8076,"fraction":92307692},"to":"PBQ7H9TRWtNXxRztKUmQKcusRwrP7tiqmpfNZn6d8X16SwsExVSzM"},{"rtt":10,"amount":{"rtt":0,"whole":1566,"fraction":25000000},"to":"PBQ7HEosipc2MYbhJLekvpGZ8eHoDtfbpTkMBVQ9SPxZwSVqQ5Dyu"},{"rtt":10,"amount":{"rtt":0,"whole":412,"fraction":5720000},"to":"PBQ7HUDAtbpnHcJtAiA551abUrj6AFqUJEX4friT3M6kHXtmxcdHR"},{"rtt":10,"amount":{"rtt":0,"whole":315,"fraction":0},"to":"PBQ7Hi8XuF1HeG7LRf3rqBSPg2Jif4h33b58zYcJHCCSZRq9uyj3m"},{"rtt":10,"amount":{"rtt":0,"whole":4716,"fraction":0},"to":"PBQ7J6aNp6qqMPnwkNzXb2pMFHMushHFB2zW1oHcAmNKZE3RKonM4"},{"rtt":10,"amount":{"rtt":0,"whole":962,"fraction":50000000},"to":"PBQ7JQtX4Ek3TtRgzyv9VA93gcxWMX1VufrujETeXSuBkZHdwy7yq"},{"rtt":10,"amount":{"rtt":0,"whole":6562,"fraction":50000000},"to":"PBQ7K7Qyv3BUwAHQnVH9nPLF3N1J1LHqr2EXM7j2Dup7AQNN12sPD"},{"rtt":10,"amount":{"rtt":0,"whole":8000,"fraction":0},"to":"PBQ7L3QBFYqcBgKp4exZZ8xsesyuB4Ag962zSmxqZMQvM1yXxP3Kg"},{"rtt":10,"amount":{"rtt":0,"whole":3255,"fraction":0},"to":"PBQ7LbNuz9Aa8VEYR5Dg5TcwceTFc1j1QvcTip4wNAufv5f6V3XMw"},{"rtt":10,"amount":{"rtt":0,"whole":559860,"fraction":0},"to":"PBQ7PLy85EwjxboGdr3w8i933KhRUwPwyWnjZTCLQZRP3zkeE8u8x"},{"rtt":10,"amount":{"rtt":0,"whole":800,"fraction":0},"to":"PBQ7PkEtENvEnstD8Exq2MGdtLm82gbGQR87HLzaxCs5yWhNi7HF9"},{"rtt":10,"amount":{"rtt":0,"whole":4200,"fraction":0},"to":"PBQ7PpzhCovGgSpDW6kH9X7xvMhZg4eoppSzceWQYLxyfrb68GJGe"},{"rtt":10,"amount":{"rtt":0,"whole":1600,"fraction":0},"to":"PBQ7QYN52JPRxwJFyMgGb77eCuQ1YLqN9eARCGMAEQbyALQ1NcyFW"},{"rtt":10,"amount":{"rtt":0,"whole":50000,"fraction":0},"to":"PBQ7S7onK6rtrpFr9wNZagk3GMCJSK5DFKWs5Ni8D1dvo7ftzvWvk"},{"rtt":10,"amount":{"rtt":0,"whole":2625,"fraction":0},"to":"PBQ7ST5aaYM5Jx8DQ1ggpUgp8iXkdbtvo9SfXjMsfcJn3ALGUtSH3"},{"rtt":10,"amount":{"rtt":0,"whole":39375,"fraction":0},"to":"PBQ7ToRXUp7xVatxikRjK7pwCR1pi9CQwKhNDHDE2nBwWaoWqNFzn"},{"rtt":10,"amount":{"rtt":0,"whole":497,"fraction":70000000},"to":"PBQ7UQ2Tjoapi8vc5irwLyBLT5CTCgrkz8KRP8R8d8CNeHHq3qyYs"},{"rtt":10,"amount":{"rtt":0,"whole":59949999,"fraction":98000000},"to":"PBQ7VVS2JvqardqQ3hvGV8ANfHEQNn2SxC3RHKoHkGmzx8moRjFHy"},{"rtt":10,"amount":{"rtt":0,"whole":800,"fraction":0},"to":"PBQ7WJv1A22rUAZcnoqFtvrEo7g5bwYicUfUjdcjJv3fBsjpRn7hg"},{"rtt":10,"amount":{"rtt":0,"whole":0,"fraction":0},"to":"PBQ7XsypoMUi82mWgiupq1JdEZaHZE1euUPy54Qci6C8jafDsyUED"},{"rtt":10,"amount":{"rtt":0,"whole":1449,"fraction":96180000},"to":"PBQ7YyVPxzh3tkSHXCTYWujumn16b28Aw5NwrST3zkVqzxhtT93e8"},{"rtt":10,"amount":{"rtt":0,"whole":1841,"fraction":62125000},"to":"PBQ7Z9PrbrkvzjsXfSp3wD38B5VUFs8rkT5iwhPumYcdJzX9z3Tot"},{"rtt":10,"amount":{"rtt":0,"whole":0,"fraction":93000000},"to":"PBQ7a6cMzjX8Ph5vTtwWGb9bVTiDQcfDiuJ9BrxzdLPksAwY5zNda"},{"rtt":10,"amount":{"rtt":0,"whole":1750,"fraction":0},"to":"PBQ7bk8kgKaJ2PPngK3Ar1wUR8Mh8dTSy1HX9Z6DvqAaCnGiGQiQQ"},{"rtt":10,"amount":{"rtt":0,"whole":136682259,"fraction":16002401},"to":"PBQ7cGUNdApH4e958Nbj9WfEwmcjLUsFk88tz6TJNGtNuJ6WXRiKz"},{"rtt":10,"amount":{"rtt":0,"whole":700,"fraction":36417500},"to":"PBQ7fhMxpkm3VaA8SzaLa67QvqxNfq4wYMq1PvHUjRYZzmm2WdAYT"},{"rtt":10,"amount":{"rtt":0,"whole":160,"fraction":41666667},"to":"PBQ7gATh6ohfQBzoLvFvNQo1zKByq9xUeHwFuGEeFhSe4BLbczTQV"},{"rtt":10,"amount":{"rtt":0,"whole":1656,"fraction":0},"to":"PBQ7hu4YmyiWQb5F2kfax2X6qNd5eBsdhMuqw5rKWmhoG1TSxbVLo"},{"rtt":10,"amount":{"rtt":0,"whole":1050,"fraction":0},"to":"PBQ7i163KNKxwZE9knf4ayfEAzqEUcdkjYXhQ93XbVKvrKdsdyL3E"},{"rtt":10,"amount":{"rtt":0,"whole":536,"fraction":19439565},"to":"PBQ7j6tzrL24rEJ1qrmWripdbeY9AjoMJroHeXQj2cZx4VPTLtwmh"},{"rtt":10,"amount":{"rtt":0,"whole":514,"fraction":77300000},"to":"PBQ7oHDj3E8PpkpY7N8LdVydUZ2idXPYCD5kF5jj5kwvhzEXD9Wo4"},)genesis"
                                 R"genesis({"rtt":10,"amount":{"rtt":0,"whole":4583,"fraction":33333333},"to":"PBQ7qEuaCzhrFyX212KA3BpXuvpYn7FrC3rLnk1grv1xtnAFgDyTQ"},{"rtt":10,"amount":{"rtt":0,"whole":1155,"fraction":0},"to":"PBQ7qT68c2pFSihT3GQvhdHS6gadbuVG4gSidh547WvBm11W5utVn"},{"rtt":10,"amount":{"rtt":0,"whole":218,"fraction":75000000},"to":"PBQ7qVvfFhZDx3Hq3NrbBUQFWR8nqbc69qt8qYujDxR9NgDYL5Cbo"},{"rtt":10,"amount":{"rtt":0,"whole":724,"fraction":50000000},"to":"PBQ7qg5YwhXhwGCwULWzb8NcuS3yL1K5u1pfnRkukw3sSSMfXirb2"},{"rtt":10,"amount":{"rtt":0,"whole":795,"fraction":99625000},"to":"PBQ7qjNt27SMAL2GwhF4s2RbY55Rf4ggvZTFXrjBj3x8wbxQzuX1C"},{"rtt":10,"amount":{"rtt":0,"whole":2000,"fraction":0},"to":"PBQ7rK4vM4d2u2fxLszGLAN3AdgMoPpZF62TpN9Np5G5Nyx2LrKr6"},{"rtt":10,"amount":{"rtt":0,"whole":40000000,"fraction":0},"to":"PBQ7rnCF7htZsQmChm8dMm8eL7hoJMoTEnJqQheEbHKWBBKeZAibM"},{"rtt":10,"amount":{"rtt":0,"whole":0,"fraction":0},"to":"PBQ7tVxiUQUsUqTyiSPAdo5Vm2Y9Kuc51RwcxJaVdDpt94AaBZQ3x"},{"rtt":10,"amount":{"rtt":0,"whole":26145,"fraction":0},"to":"PBQ7vLr5j7dUAKt5mFtsDZyzBRhoPu8L4x6SiKjpm41hUiEEHndj7"},{"rtt":10,"amount":{"rtt":0,"whole":231455,"fraction":42000000},"to":"PBQ7vcaR7tERpACGHgY4daDuvvkwZaxYP8faR867X16vAwx4zfyEu"},{"rtt":10,"amount":{"rtt":0,"whole":1050000,"fraction":0},"to":"PBQ7w96cnNj4GcpSk1vPHs5iZEqSot5hZM2vEgwWdv2FwxN4sdeSA"},{"rtt":10,"amount":{"rtt":0,"whole":800,"fraction":0},"to":"PBQ7wwhw7P3ZB3NoyYRzLoAjbQNySYPeECLMSbz3rBFBi2e8uRHKE"},{"rtt":10,"amount":{"rtt":0,"whole":78166,"fraction":44954079},"to":"PBQ7yCS9V1RaFLvoPNAhSdqJzY73fF5Uu83SxabdMDvBQAMCHg5Cc"},{"rtt":10,"amount":{"rtt":0,"whole":175,"fraction":45017000},"to":"PBQ81mg3gdb3TeFWGdw16pNGW5CMsCSx8zzNHzRsz3mJmfF5NKuwn"},{"rtt":10,"amount":{"rtt":0,"whole":1869,"fraction":0},"to":"PBQ82W1LtQq5Fzbc5AzBd3j3V8M7JFVVNTzxris2iQvKcRgw4AwEx"},{"rtt":10,"amount":{"rtt":0,"whole":0,"fraction":0},"to":"PBQ83QGnbrGvtJa8zm936Eyh58ogC6Vz6CXFDLLgqbMFapCzTcV4o"},{"rtt":10,"amount":{"rtt":0,"whole":2398,"fraction":1000000},"to":"PBQ843nPbffhnTihNjdcAW9cDbnvNEAaqUua2Tw2CRYoumVy9Fm65"},{"rtt":10,"amount":{"rtt":0,"whole":239,"fraction":70041667},"to":"PBQ86txjUmscnGZcTKE2LNEm3auy46kEGEQs9H4PGMZ7gXnWdUHrF"},{"rtt":10,"amount":{"rtt":0,"whole":6000,"fraction":0},"to":"PBQ882Ciw2Z564vYwsTyjCegVkwxzTBYc7BYYc5XvLuU1JA4xaGch"},{"rtt":10,"amount":{"rtt":0,"whole":1523,"fraction":52547500},"to":"PBQ89uf8NNdCvBWg3X8wQMB9Mx5sAzZwHmiJoNAeWGwPSRowLTJvR"},{"rtt":10,"amount":{"rtt":0,"whole":1754,"fraction":37818182},"to":"PBQ8BQcmEcUjKCYrCLArmNf21bdSXcJpd8Aw6tUvtxrKobfS5MzKh"},{"rtt":10,"amount":{"rtt":0,"whole":105,"fraction":0},"to":"PBQ8CMr6wEn2RGZrMKKuKnSXrntKyVvZ12Tqgth7HrFo2GMgCY7Zp"},{"rtt":10,"amount":{"rtt":0,"whole":800,"fraction":0},"to":"PBQ8ChD2Zs3C9YAxsFbwDRQpHs4vAksfVdWUDrecJUrzMU9cfNRMX"},{"rtt":10,"amount":{"rtt":0,"whole":514,"fraction":50000000},"to":"PBQ8ChMmCfqsSG7LjY5m7r7TQfsVztaB1Cm4kyHc38UMcsmECEjin"},{"rtt":10,"amount":{"rtt":0,"whole":0,"fraction":0},"to":"PBQ8F5tvKn8uN8wuQJedg8y6KGuwy38pvyeP69x17A1xbAiFbTJYz"},{"rtt":10,"amount":{"rtt":0,"whole":363000,"fraction":0},"to":"PBQ8Fk2wKHKw9DPVPTAoCepm46YKLdxfwaVPmNpyN6VSeAKFiTrcC"},{"rtt":10,"amount":{"rtt":0,"whole":200000,"fraction":0},"to":"PBQ8GbgQeNPo3ci76AEXqFGpJxYT1HDRDyyqHHKChoB1js5GarwTY"},{"rtt":10,"amount":{"rtt":0,"whole":0,"fraction":0},"to":"PBQ8HmSg3PhBxm5kRnJjp1FdWG66SDfMj3JfQMebDLaDDsg4FPGSN"},{"rtt":10,"amount":{"rtt":0,"whole":388,"fraction":6680000},"to":"PBQ8JTuNGxLq3GdbR66j2w4W9DGdJ7pAfxjbtGTthwKkffSTMrEQc"},{"rtt":10,"amount":{"rtt":0,"whole":1260,"fraction":0},"to":"PBQ8Kb87SqdLyMKPajnEBU5LSSoeFXQwAVgtVzjT7YqX7K5MiJwRC"},{"rtt":10,"amount":{"rtt":0,"whole":1080,"fraction":0},"to":"PBQ8Lc91xkkiced1oP2fLXfwUut1bpy8LnEw1imTBJe2UipDXAcrd"},{"rtt":10,"amount":{"rtt":0,"whole":954,"fraction":54545455},"to":"PBQ8NBsEo4pL3fZBnXBJsvdUjx8ZcsHsKXzkJVdHVfRRAoRWLzjZP"},{"rtt":10,"amount":{"rtt":0,"whole":6125,"fraction":0},"to":"PBQ8PFfF917PR1U85Vi7X2xCQRWk9gPqyP5nc9enjY2gEbasM2p11"},{"rtt":10,"amount":{"rtt":0,"whole":714,"fraction":8810000},"to":"PBQ8QeCgy25apgyHfPxEEERCV5XBFRNyFuTpdcN5brLd42dLJg9UN"},{"rtt":10,"amount":{"rtt":0,"whole":500,"fraction":0},"to":"PBQ8Rgb17BgMffASVfHPqTYBd1A5AD7v5EAKRSNKap2c73bRBQR72"},{"rtt":10,"amount":{"rtt":0,"whole":1600,"fraction":0},"to":"PBQ8RmUCiGML2Sm3vzBzbmg2SzqcDtpfkYDfCFd964YDGey9BMgjg"},{"rtt":10,"amount":{"rtt":0,"whole":649,"fraction":75785000},"to":"PBQ8SUFrf6pizfwqJFuimf1oR6yRgMoUVRW1zVNJkt2iJQXZPiqRY"},{"rtt":10,"amount":{"rtt":0,"whole":0,"fraction":48000000},"to":"PBQ8Si5N2j7GEst27u9tdqYfk7CrPtTjb6VN7fESDBLZ6U4vDDfj8"},{"rtt":10,"amount":{"rtt":0,"whole":9545,"fraction":45454545},"to":"PBQ8Vqthzx7WoRzD3ik6KvriQ7dbhMx8Zde8ZaRT564LcGZrhGV1A"},{"rtt":10,"amount":{"rtt":0,"whole":800,"fraction":0},"to":"PBQ8XqYCp7Mc1EHpvba7vqPDxGnPQHye7KNRrtLBF9SZjDDAUryA3"},{"rtt":10,"amount":{"rtt":0,"whole":1050,"fraction":0},"to":"PBQ8ZQ2KBMTQH6DVthJ4pt5Exk6LL53rzjcC5i75jwXn1p7P9BxvW"},{"rtt":10,"amount":{"rtt":0,"whole":2993,"fraction":79150000},"to":"PBQ8ZvXJpXy2j7tqhYVGmPkiUFDPUygNg6JkJ5SaV8YxuPDZKKwtv"},{"rtt":10,"amount":{"rtt":0,"whole":800,"fraction":0},"to":"PBQ8aQ77xoBrDPq8GQXSyuH3rc4Dz8dtkDDsHdUqNEZxYm7kT9s77"},{"rtt":10,"amount":{"rtt":0,"whole":800,"fraction":0},"to":"PBQ8arufDJGfdLtm7hNiryxzpfiAhKTKnhwxsMjuPMgiQaAak7qpF"},{"rtt":10,"amount":{"rtt":0,"whole":1050,"fraction":0},"to":"PBQ8b3mMKSpYSpNUsfWMJBPRAjk1phjc99ohQw1DnkYBv4AKoD6Cq"},{"rtt":10,"amount":{"rtt":0,"whole":12000,"fraction":0},"to":"PBQ8cmiRzYjwzQ913s6KPofGLa2zHjXTFuyAUxgHkzpPT6eyMGVzt"},{"rtt":10,"amount":{"rtt":0,"whole":299,"fraction":99931818},"to":"PBQ8f9HVYmFsyh1g1nurBip6o1Mgwst1vfiJyZtWGmbLtweaHUevn"},{"rtt":10,"amount":{"rtt":0,"whole":1056,"fraction":48585000},"to":"PBQ8iAstuLUfKh7nz5xU16X7oBtUyKmLebPZVke1o68aDWUm3LG3b"},{"rtt":10,"amount":{"rtt":0,"whole":100,"fraction":74000000},"to":"PBQ8igqkU49nPW3NjWKnqmSiZheFC8GLoy7NiWUCii8Q7P1ZLEZ46"},{"rtt":10,"amount":{"rtt":0,"whole":800,"fraction":0},"to":"PBQ8jSLsjcDkDZ1auJApaurwUDCcyHUBQe8zxbEsnHVJJaTnTv9QC"},{"rtt":10,"amount":{"rtt":0,"whole":4772,"fraction":72727272},"to":"PBQ8juWRKhQn6xC8CctdEjECTXy82MNNiMoyLzrVfDUWKkDYG53ww"},{"rtt":10,"amount":{"rtt":0,"whole":120000,"fraction":0},"to":"PBQ8kA7dersa7x6CGm5khd5CTTvGTeaKWSWTFRCe3RqGQv82C7j89"},{"rtt":10,"amount":{"rtt":0,"whole":505,"fraction":0},"to":"PBQ8mmYhiyad8jSvEppePtM1Kquub6aiDLqjQWdwDVdyvbKwHZqQb"}],"signed_transactions":[]},"authority":"PBQ5HnbMEwb8AYsqZrrEwPaKZ1kzADmwuUMhhtdhL5ZdCCW5pkWmq","signature":"AN1rKvtNefWH5dX2JDK24uJM9vWj5J3cRKSJpwMdKzrcWPBZb3sk2Jqb9iqVrQJCnhsmUooFsmfSwDfB8jqqU1bFSC3fdPeDr"})genesis"
                                 );

        beltpp::on_failure guard([&] { discard(); });

        // apply rewards to state and action_log
        for (auto const& item : signed_block.block_details.rewards)
            m_state.increase_balance(item.to, item.amount);

        // insert to blockchain and action_log
        m_blockchain.insert(signed_block);
        m_action_log.log_block(signed_block);

        save(guard);
        calc_balance();
        calc_sync_info(signed_block.block_details);
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
    unique_ptr<beltpp::ip_address> m_ptr_external_address;

    beltpp::timer m_sync_timer;
    beltpp::timer m_check_timer;
    beltpp::timer m_broadcast_timer;
    beltpp::timer m_cache_cleanup_timer;
    beltpp::timer m_summary_report_timer;

    beltpp::ip_address m_rpc_bind_to_address;

    publiqpp::blockchain m_blockchain;
    publiqpp::action_log m_action_log;
    publiqpp::storage m_storage;
    publiqpp::transaction_pool m_transaction_pool;
    publiqpp::state m_state;

    session_manager m_sessions;

    unordered_set<beltpp::isocket::peer_id> m_p2p_peers;
    unordered_map<beltpp::isocket::peer_id, packet_and_expiry> m_stored_requests;
    unordered_map<string, system_clock::time_point> m_transaction_cache;

    bool m_miner;
    Coin m_balance;
    NodeType m_node_type;
    meshpp::private_key m_pv_key;
    meshpp::public_key m_pb_key;
    stat_counter m_stat_counter;
    ip_address public_address;

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
