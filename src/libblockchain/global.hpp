#pragma once

#include <mesh.pp/global.hpp>

#if defined(BLOCKCHAIN_LIBRARY)
#define BLOCKCHAINSHARED_EXPORT MESH_EXPORT
#else
#define BLOCKCHAINSHARED_EXPORT MESH_IMPORT
#endif

// Blocks and headers max count per one request
#define TRANSFER_LENGTH 10

// Block mine delay in seconds
#define BLOCK_MINE_DELAY 300
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

#define MINER_REWARD 100
#define MINE_AMOUNT_THRESHOLD 1
