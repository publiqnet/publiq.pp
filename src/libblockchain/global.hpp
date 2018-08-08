#pragma once

#include <mesh.pp/global.hpp>

#if defined(BLOCKCHAIN_LIBRARY)
#define BLOCKCHAINSHARED_EXPORT MESH_EXPORT
#else
#define BLOCKCHAINSHARED_EXPORT MESH_IMPORT
#endif
