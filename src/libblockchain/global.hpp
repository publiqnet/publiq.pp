#pragma once

#include <mesh.pp/global.hpp>

#if defined(BLOCKCHAIN_LIBRARY)
#define BLOCKCHAINSHARED_EXPORT MESH_EXPORT
#else
#define BLOCKCHAINSHARED_EXPORT MESH_IMPORT
#endif

//temp
#define DELTA_STEP  10
#define DELTA_MAX   120000000
#define DELTA_UP    100000000
#define DELTA_DOWN  80000000

