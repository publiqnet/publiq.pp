#pragma once

#include <publiq.pp/global.hpp>

#if defined(SIMULATOR_LIBRARY)
#define SIMULATORSHARED_EXPORT PUBLIQ_EXPORT
#else
#define SIMULATORSHARED_EXPORT PUBLIQ_IMPORT
#endif
