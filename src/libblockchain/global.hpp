#pragma once

#include <publiq.pp/global.hpp>

#if defined(BLOCKCHAIN_LIBRARY)
#define BLOCKCHAINSHARED_EXPORT PUBLIQ_EXPORT
#else
#define BLOCKCHAINSHARED_EXPORT PUBLIQ_IMPORT
#endif
