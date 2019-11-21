#pragma once

#include "global.hpp"

#include <belt.pp/ilog.hpp>
#include <belt.pp/isocket.hpp>

#include <mesh.pp/cryptoutility.hpp>

#include <boost/filesystem/path.hpp>

#include <memory>
#include <string>
#include <chrono>
#include <map>

namespace storage_utility
{
namespace detail
{
class rpc_internals;
}

class STORAGEUTILITYSHARED_EXPORT rpc
{
public:
    rpc(beltpp::ip_address const& rpc_bind_to_address,
        beltpp::ilog* plogger_node);
    rpc(rpc&& other) noexcept;
    ~rpc();

    void wake();
    bool run();

private:
    std::unique_ptr<detail::rpc_internals> m_pimpl;
};

}

