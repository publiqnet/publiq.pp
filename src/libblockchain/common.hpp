#pragma once

#include "coin.hpp"

// Blocks and headers max count per one request - 1
// corners are included
#define TRANSFER_LENGTH 9

// Block mine delay in seconds
#define BLOCK_MINE_DELAY 60
// Sync process request/response maximum dely
#define SYNC_STEP_TIMEOUT 3

// Timers
#define CHECK_TIMER 1
#define SYNC_TIMER  10
#define EVENT_TIMER 30

// Consensus delta definitions
#define DELTA_STEP  10
#define DELTA_MAX   120000000
#define DELTA_UP    100000000
#define DELTA_DOWN  80000000

static const coin MINER_REWARD(1, 0);
static const coin MINE_AMOUNT_THRESHOLD(1, 0);

