#pragma once

#include <mesh.pp/global.hpp>

#if defined(BLOCKCHAIN_LIBRARY)
#define BLOCKCHAINSHARED_EXPORT MESH_EXPORT
#else
#define BLOCKCHAINSHARED_EXPORT MESH_IMPORT
#endif

// Blocks and headers max count per one request
#define TRANSFER_LENGTH 10

// Block mine delay in minutes
#define BLOCK_MINE_DELAY 10
// Step apply delay in seconds
#define BLOCK_APPLY_DELAY 30
// Sync process request/response maximum dely
#define SYNC_STEP_TIMEOUT 3

// Consensus delta definitions
#define DELTA_STEP  10
#define DELTA_MAX   120000000
#define DELTA_UP    100000000
#define DELTA_DOWN  80000000

#define MINE_THRESHOLD 100000000
