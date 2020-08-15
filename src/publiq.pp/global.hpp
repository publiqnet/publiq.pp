#pragma once

#include <belt.pp/global.hpp>
#include <string>

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

namespace publiqpp
{
inline std::string version_string(std::string const& program_name)
{
    return "PUBLIQ Foundation\n"
           "publiq.pp " PUBLIQPP_VERSION "\n" +
            program_name;
}
}