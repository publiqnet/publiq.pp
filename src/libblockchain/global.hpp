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
#define MINE_DELAY 10
// Step apply delay in seconds
#define STEP_DELAY 30

// Consensus delta definitions
#define DELTA_STEP  10
#define DELTA_MAX   120000000
#define DELTA_UP    100000000
#define DELTA_DOWN  80000000
