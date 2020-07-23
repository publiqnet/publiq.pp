#pragma once

//#define EXTRA_LOGGING

#include "coin.hpp"
#include "message.hpp"

#include <belt.pp/ievent.hpp>
#include <belt.pp/socket.hpp>
#include <belt.pp/packet.hpp>

#include <mesh.pp/p2psocket.hpp>

#include <string>
#include <vector>
#include <unordered_map>
#include <utility>

std::string const node_peerid = "node";
std::string const storage_peerid = "storage";

// Blocks and headers max count per one request - 1
// corners are included
#define BLOCK_TR_LENGTH 9
#define HEADER_TR_LENGTH 49

// Maximum buffer length of blocks
// that can be collected per sync
#define BLOCK_INSERT_LENGTH 50
#define BLOCK_REVERT_LENGTH 50

// Block mine delay in seconds
#define BLOCK_MINE_DELAY 600
#define BLOCK_WAIT_DELAY 120
#define BLOCK_SAFE_DELAY 240

// Sent packet will considered as not answered
// after PACKET_EXPIRY_STEPS x EVENT_TIMER seconds
#define PACKET_EXPIRY_STEPS 60

// Block maximum transactions count
#define BLOCK_MAX_TRANSACTIONS 1000ull

// Action log max response count
#define ACTION_LOG_MAX_RESPONSE 10000

// Max chunk size of files to request and process at a time
#define STORAGE_MAX_FILE_REQUESTS 100

// Timers in seconds
#define CHECK_TIMER 1
#define SYNC_TIMER  30
#define EVENT_TIMER 20
#define BROADCAST_TIMER 1800
#define CACHE_CLEANUP_TIMER 300
#define SUMMARY_REPORT_TIMER 1800

#define TRANSACTION_MAX_LIFETIME_HOURS 24

// Maximum time shift on seconds
// acceptable between nodes
#define NODES_TIME_SHIFT 60

#define PUBLIC_ADDRESS_FRESH_THRESHHOLD_SECONDS 600

// Consensus delta definitions
#define DELTA_STEP  3ull
#define DELTA_MAX   7000000000ull
#define DELTA_UP    4000000000ull
#define DELTA_DOWN  1000000000ull

#define DIST_MAX    4294967296ull

// Service statistics acceptable discreancy
#define STAT_ERROR_LIMIT 1.2

// Reward coins percents
#define MINER_EMISSION_REWARD_PERCENT    10
#define AUTHOR_EMISSION_REWARD_PERCENT   40
#define CHANNEL_EMISSION_REWARD_PERCENT  30
#define STORAGE_EMISSION_REWARD_PERCENT  20

#define AUTHOR_SPONSORED_REWARD_PERCENT  50
#define CHANNEL_SPONSORED_REWARD_PERCENT 30
#define STORAGE_SPONSORED_REWARD_PERCENT 20

using coin = publiqpp::coin;

#define CHANNEL_AMOUNT_THRESHOLD    coin(100000, 0)
#define STORAGE_AMOUNT_THRESHOLD    coin(10000, 0)

namespace publiqpp
{

namespace detail
{
inline
beltpp::void_unique_ptr get_putl()
{
    beltpp::message_loader_utility utl;
    BlockchainMessage::detail::extension_helper(utl);

    auto ptr_utl =
        beltpp::new_void_unique_ptr<beltpp::message_loader_utility>(std::move(utl));

    return ptr_utl;
}

inline 
std::string peer_short_names(std::string const& peerid)
{
    if (peerid == "TPBQ7JEFjtQNjyzwnThepF2jJtCe7cCpUFEaxGdUnN2W9wPP5Nh92G")
        return "$tigran(0)";
    if (peerid == "TPBQ8gyokoWdo9tSLcDQQjxdhYgmmnScUPT6YDCaVVoeSFRz1zkGpv")
        return "$tigran(1)";
    if (peerid == "TPBQ5LNw1peEL8ZRDEw6ukndHpaob8A43dsh2beYg9cwocHm5r3tPR")
        return "$tigran(2)";
    if (peerid == "TPBQ5pFSs7NKc26b3gpeFN17oGYkn3vFEuf8sA4HhZQsF9MfRrXShC")
        return "$tigran(3)";
    if (peerid == "TPBQ5Nd79pnM2X6E8NTPPwMXBrX8XigztwU3L51ALPSVBQH2L8tiZw")
        return "$tigran(4)";
    if (peerid == "TPBQ4te6LkpCnsu9DyoRUZpmhMypbMwqrpofUWvRgGanY8c2vYciwz")
        return "$tigran(5)";
    if (peerid == "TPBQ76Zv5QceNSLibecnMGEKbKo3dVFV6HRuDSuX59mJewJxHPhLwu")
        return "$armen(0)";
    if (peerid == "TPBQ7aYzUMXfRcmho8wDwFk1oFyGopjD6ADWG7JR4DxvfJn392mpe4")
        return "$armen(1)";
    if (peerid == "TPBQ8MiwBdYzSj38etLYLES4FSuKJnLPkXAJv4MyrLW7YJNiPbh4z6")
        return "$sona(0)";
    if (peerid == "TPBQ8VLQxxbfD8SNp5LWy2y8rEvLsqcLpKsWCdKqhAEgsjpyhNVqkf")
        return "$sona(1)";
    if (peerid == "TPBQ8f5Z8SKVrYFES1KLHtCYMx276a5NTgZX6baahzTqkzfnB4Pidk")
        return "$gagik(0)";
    if (peerid == "TPBQ87WZycpRYUWcVC9wB3PL5QgYiZRh3Adg8FWAjtTo2GykFj3anC")
        return "$gagik(1)";
    if (peerid == "TPBQ7Ta31VaxCB9VfDRvYYosKYpzxXNgVH46UkM9i4FhzNg4JEU3YJ")
        return "$node(0)";
    if (peerid == "TPBQ4vj4CpQ11HTWg7wSFY3cg5gR4qBxgJJi2uSNJGNTmF22qt5Mbg")
        return "$state(0)";

    // test channels
    if (peerid == "TPBQ7MJwGCpZStXbTsukEbunryBKaBL6USPY6n2FyJA97uCiHkvasp")
        return "Channel 1";
    if (peerid == "TPBQ55JmktyTGavdkoNyYKaGyQc3MoPKeF9GJm9B8DSKyR8wdasLFi")
        return "Channel 2";
    if (peerid == "TPBQ6QhGD13EVcNPyCoh5TDrMg4qjLEie3bY4TSHguzieGaU76uBTW")
        return "Channel 3";
    if (peerid == "TPBQ5JS7M2FvurTN6Z4xvArrsTeGkEvozxBynn72mqGvBxFGDkc7sF")
        return "Channel 4";
    if (peerid == "TPBQ6fRwbcEhy6V7szQcWi3xZGjgSyfc5rG2xJZkEwGDvN911aabqB")
        return "Channel 5";
    if (peerid == "TPBQ81SSqnfubb49evai9mXNnMKPGbpf1NogRNKfyc4cKZiT4zUt82")
        return "Channel 6";
    if (peerid == "TPBQ7LeKRHY74LZxremBwGUUYBwahmxF667e5K6QydSRhnMMWjtbpR")
        return "Channel 7";
    if (peerid == "TPBQ7ed4zmD1oMkCNmtTcL2xovaEPFUCFD2CQTWeJjTsS394ZHcnLS")
        return "Channel 8";
    if (peerid == "TPBQ7e2QM4PGk4GdFHULGQ8D5pDZEZiUDAEh4giyfoCxE38DmFcuZx")
        return "Channel 9";

    // test storages
    if (peerid == "TPBQ8mogwgutXv1hJnuYXrQF9z6AeMU69TpVeau7qhwoshCEpSQEbt")
        return "Storage 1";
    if (peerid == "TPBQ8NBGvuRuaMgWZaehdj5RyZ3eYPoVGqtQPxGL7cpSD4buMNHnoj")
        return "Storage 2";
    if (peerid == "TPBQ7DZupx4cFDRDLUdfv6kBEcam3HUxJEb12tJcd9YXEUkJqnHBpo")
        return "Storage 3";
    if (peerid == "TPBQ5YeuYa2Gx46JScNBLodo3rpYGGsvpXanBEa8xDMuTnBLSZ2dUF")
        return "Storage 4";
    if (peerid == "TPBQ78f9mssRrSCC7vPeRQDURXB4v4GrNt2MRa9hUpxxSMWvVuiXVw")
        return "Storage 5";
    if (peerid == "TPBQ5Jq6NAcUSvToXGYGBxrFCeptBbiZLDjsiGdAPC6ghpT2Pycdq2")
        return "Storage 6";
    if (peerid == "TPBQ4va3AgdbeCZaeLeijyAUC9XhUKceLfS65ZaKYCgTgHsxorBM64")
        return "Storage 7";
    if (peerid == "TPBQ7ifcSMvBeBPcjDfPb9hPxJfJjNAsvHMX6UtzRKwSnuBYVLjMMy")
        return "Storage 8";
    if (peerid == "TPBQ5ouJXAaRiXbBwZuoXr4Qgn4MPaA6aE44nbJLrRyg6r1FJFBr7G")
        return "Storage 9";

    return peerid;
}

class wait_result_item
{
public:
    enum event_type {nothing, event, timer};
    event_type et = nothing;
    beltpp::event_item const* pevent_item = nullptr;
    beltpp::socket::peer_id peerid;
    beltpp::packet packet;

    static wait_result_item event_result(beltpp::event_item const* pevent_item,
                                         beltpp::socket::peer_id const& peerid,
                                         beltpp::packet&& packet)
    {
        wait_result_item res;
        res.et = event;
        res.pevent_item = pevent_item;
        res.peerid = peerid;
        res.packet = std::move(packet);

        return res;
    }

    static wait_result_item timer_result()
    {
        wait_result_item res;
        res.et = timer;

        return res;
    }

    static wait_result_item empty_result()
    {
        wait_result_item res;
        res.et = nothing;

        return res;
    }
};

class wait_result
{
public:
    using packets_result = std::pair<beltpp::socket::peer_id, beltpp::socket::packets>;
    using event_result = std::pair<beltpp::event_item const*, packets_result>;
    using event_results = std::unordered_map<beltpp::event_item const*, packets_result>;

    beltpp::event_handler::wait_result m_wait_result = beltpp::event_handler::wait_result::nothing;
    event_results event_packets;
};

wait_result_item wait_and_receive_one(wait_result& wait_result_info,
                                      beltpp::event_handler& eh,
                                      beltpp::stream* rpc_stream,
                                      meshpp::p2psocket* p2p_stream,
                                      beltpp::stream* on_demand_stream);

}
}
