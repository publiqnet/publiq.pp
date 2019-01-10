#pragma once

#include "coin.hpp"
#include "message.hpp"
#include "storage_message.hpp"

#include <string>
#include <vector>

// Blocks and headers max count per one request - 1
// corners are included
#define BLOCK_TR_LENGTH 9
#define HEADER_TR_LENGTH 49

// Maximum buffer length of blocks
// which can be cillected for sync
#define BLOCK_INSERT_LENGTH 50

// Block mine delay in seconds
#define BLOCK_MINE_DELAY 600
#define BLOCK_WAIT_DELAY 180

// Sync process request/response maximum dely
#define SYNC_FAILURE_TIMEOUT 30

// Sent packet will considered as not answered
// after PACKET_EXPIRY_STEPS x EVENT_TIMER seconds
#define PACKET_EXPIRY_STEPS 60

// Block maximum transactions count
#define BLOCK_MAX_TRANSACTIONS 1000

// Action log max response count
#define ACTION_LOG_MAX_RESPONSE 10000

// Timers in seconds
#define CHECK_TIMER 1
#define SYNC_TIMER  15
#define EVENT_TIMER 5
#define BROADCAST_TIMER 1800
#define CACHE_CLEANUP_TIMER 300
#define SUMMARY_REPORT_TIMER 1800

#define TRANSACTION_MAX_LIFETIME_HOURS 24

// Maximum time shift on seconds
// acceptable between nodes
#define NODES_TIME_SHIFT 60

// Consensus delta definitions
#define DELTA_STEP  10ull
#define DELTA_MAX   120000000ull
#define DELTA_UP    100000000ull
#define DELTA_DOWN  80000000ull

#define DIST_MAX    4294967296ull

// Reward coins percents
#define MINER_REWARD_PERCENT    10
#define STORAGE_REWARD_PERCENT  30
#define CHANNEL_REWARD_PERCENT  60

static const coin MINE_AMOUNT_THRESHOLD(1, 0);

static const std::vector<coin> BLOCK_REWARD_ARRAY
{
    coin(1000,0),     coin(800,0),      coin(640,0),        coin(512,0),        coin(410,0),        coin(327,0),
    coin(262,0),      coin(210,0),      coin(168,0),        coin(134,0),        coin(107,0),        coin(86,0),
    coin(68,0),       coin(55,0),       coin(44,0),         coin(35,0),         coin(28,0),         coin(22,0),
    coin(18,0),       coin(15,0),       coin(12,0),         coin(9,0),          coin(7,0),          coin(6,0),
    coin(5,0),        coin(4,0),        coin(3,0),          coin(2,50000000),   coin(2,0),          coin(1,50000000),
    coin(1,20000000), coin(1,0),        coin(0,80000000),   coin(0,70000000),   coin(0,60000000),   coin(0,50000000),
    coin(0,40000000), coin(0,30000000), coin(0,20000000),   coin(0,17000000),   coin(0,14000000),   coin(0,12000000),
    coin(0,10000000), coin(0,8000000),  coin(0,7000000),    coin(0,6000000),    coin(0,6000000),    coin(0,5000000),
    coin(0,5000000),  coin(0,5000000),  coin(0,4000000),    coin(0,4000000),    coin(0,4000000),    coin(0,4000000),
    coin(0,4000000),  coin(0,3000000),  coin(0,3000000),    coin(0,3000000),    coin(0,3000000),    coin(0,3000000)
};

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
    if (peerid == "PBQ7JEFjtQNjyzwnThepF2jJtCe7cCpUFEaxGdUnN2W9wPP5Nh92G")
        return "$tigran(0)";
    if (peerid == "PBQ8gyokoWdo9tSLcDQQjxdhYgmmnScUPT6YDCaVVoeSFRz1zkGpv")
        return "$tigran(1)";
    if (peerid == "PBQ5LNw1peEL8ZRDEw6ukndHpaob8A43dsh2beYg9cwocHm5r3tPR")
        return "$tigran(2)";
    if (peerid == "PBQ5pFSs7NKc26b3gpeFN17oGYkn3vFEuf8sA4HhZQsF9MfRrXShC")
        return "$tigran(3)";
    if (peerid == "PBQ5Nd79pnM2X6E8NTPPwMXBrX8XigztwU3L51ALPSVBQH2L8tiZw")
        return "$tigran(4)";
    if (peerid == "PBQ4te6LkpCnsu9DyoRUZpmhMypbMwqrpofUWvRgGanY8c2vYciwz")
        return "$tigran(5)";
    if (peerid == "PBQ76Zv5QceNSLibecnMGEKbKo3dVFV6HRuDSuX59mJewJxHPhLwu")
        return "$armen(0)";
    if (peerid == "PBQ7aYzUMXfRcmho8wDwFk1oFyGopjD6ADWG7JR4DxvfJn392mpe4")
        return "$armen(1)";
    if (peerid == "PBQ8MiwBdYzSj38etLYLES4FSuKJnLPkXAJv4MyrLW7YJNiPbh4z6")
        return "$sona(0)";
    if (peerid == "PBQ8VLQxxbfD8SNp5LWy2y8rEvLsqcLpKsWCdKqhAEgsjpyhNVqkf")
        return "$sona(1)";
    if (peerid == "PBQ8f5Z8SKVrYFES1KLHtCYMx276a5NTgZX6baahzTqkzfnB4Pidk")
        return "$gagik(0)";
    if (peerid == "PBQ87WZycpRYUWcVC9wB3PL5QgYiZRh3Adg8FWAjtTo2GykFj3anC")
        return "$gagik(1)";
    if (peerid == "PBQ7Ta31VaxCB9VfDRvYYosKYpzxXNgVH46UkM9i4FhzNg4JEU3YJ")
        return "$north.publiq.network:12222";   //  node(0)
    if (peerid == "PBQ4vj4CpQ11HTWg7wSFY3cg5gR4qBxgJJi2uSNJGNTmF22qt5Mbg")
        return "$north.publiq.network:13333";   //  state(0)

    // test channels
    if (peerid == "PBQ7MJwGCpZStXbTsukEbunryBKaBL6USPY6n2FyJA97uCiHkvasp")
        return "Channel 1";
    if (peerid == "PBQ55JmktyTGavdkoNyYKaGyQc3MoPKeF9GJm9B8DSKyR8wdasLFi")
        return "Channel 2";
    if (peerid == "PBQ6QhGD13EVcNPyCoh5TDrMg4qjLEie3bY4TSHguzieGaU76uBTW")
        return "Channel 3";
    if (peerid == "PBQ5JS7M2FvurTN6Z4xvArrsTeGkEvozxBynn72mqGvBxFGDkc7sF")
        return "Channel 4";
    if (peerid == "PBQ6fRwbcEhy6V7szQcWi3xZGjgSyfc5rG2xJZkEwGDvN911aabqB")
        return "Channel 5";
    if (peerid == "PBQ81SSqnfubb49evai9mXNnMKPGbpf1NogRNKfyc4cKZiT4zUt82")
        return "Channel 6";
    if (peerid == "PBQ7LeKRHY74LZxremBwGUUYBwahmxF667e5K6QydSRhnMMWjtbpR")
        return "Channel 7";
    if (peerid == "PBQ7ed4zmD1oMkCNmtTcL2xovaEPFUCFD2CQTWeJjTsS394ZHcnLS")
        return "Channel 8";
    if (peerid == "PBQ7e2QM4PGk4GdFHULGQ8D5pDZEZiUDAEh4giyfoCxE38DmFcuZx")
        return "Channel 9";

    // test storages
    if (peerid == "PBQ8mogwgutXv1hJnuYXrQF9z6AeMU69TpVeau7qhwoshCEpSQEbt")
        return "Storage 1";
    if (peerid == "PBQ8NBGvuRuaMgWZaehdj5RyZ3eYPoVGqtQPxGL7cpSD4buMNHnoj")
        return "Storage 2";
    if (peerid == "PBQ7DZupx4cFDRDLUdfv6kBEcam3HUxJEb12tJcd9YXEUkJqnHBpo")
        return "Storage 3";
    if (peerid == "PBQ5YeuYa2Gx46JScNBLodo3rpYGGsvpXanBEa8xDMuTnBLSZ2dUF")
        return "Storage 4";
    if (peerid == "PBQ78f9mssRrSCC7vPeRQDURXB4v4GrNt2MRa9hUpxxSMWvVuiXVw")
        return "Storage 5";
    if (peerid == "PBQ5Jq6NAcUSvToXGYGBxrFCeptBbiZLDjsiGdAPC6ghpT2Pycdq2")
        return "Storage 6";
    if (peerid == "PBQ4va3AgdbeCZaeLeijyAUC9XhUKceLfS65ZaKYCgTgHsxorBM64")
        return "Storage 7";
    if (peerid == "PBQ7ifcSMvBeBPcjDfPb9hPxJfJjNAsvHMX6UtzRKwSnuBYVLjMMy")
        return "Storage 8";
    if (peerid == "PBQ5ouJXAaRiXbBwZuoXr4Qgn4MPaA6aE44nbJLrRyg6r1FJFBr7G")
        return "Storage 9";

    return peerid;
}
}
}
