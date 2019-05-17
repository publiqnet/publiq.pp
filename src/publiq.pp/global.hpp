#include <belt.pp/global.hpp>

#define PUBLIQ_EXPORT BELT_EXPORT
#define PUBLIQ_IMPORT BELT_IMPORT
#define PUBLIQ_LOCAL BELT_LOCAL

#ifdef B_OS_LINUX
#define P_OS_LINUX
#endif

#ifdef B_OS_WINDOWS
#define P_OS_WINDOWS
#endif

#ifdef B_OS_MACOS
#define P_OS_MACOS
#endif
