#pragma once

#include "coin.hpp"
#include "message.hpp"

// Blocks and headers max count per one request - 1
// corners are included
#define BLOCK_TR_LENGTH 19
#define HEADER_TR_LENGTH 59

// Maximum buffer length of blocks
// which can be cillected for sync
#define BLOCK_INSERT_LENGTH 1000

// Block mine delay in seconds
#define BLOCK_MINE_DELAY 60

// Sync process request/response maximum dely
#define SYNC_STEP_TIMEOUT 300

// Timers
#define CHECK_TIMER 1
#define SYNC_TIMER  15
#define EVENT_TIMER 30

// Transaction maximum lifetime in seconds
#define TRANSACTION_LIFETIME 86400

// Maximum time shift on seconds
// acceptable between nodes
#define NODES_TIME_SHIFT 60

// Consensus delta definitions
#define DELTA_STEP  10ull
#define DELTA_MAX   120000000ull
#define DELTA_UP    100000000ull
#define DELTA_DOWN  80000000ull

#define DIST_MAX    4294967296ull

static const coin MINER_REWARD(1, 0);
static const coin MINE_AMOUNT_THRESHOLD(1, 0);

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
}
}
