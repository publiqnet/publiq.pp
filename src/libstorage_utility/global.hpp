#pragma once

#include <publiq.pp/global.hpp>

#if defined(STORAGEUTILITY_LIBRARY)
#define STORAGEUTILITYSHARED_EXPORT PUBLIQ_EXPORT
#else
#define STORAGEUTILITYSHARED_EXPORT PUBLIQ_IMPORT
#endif
