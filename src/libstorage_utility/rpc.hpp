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
        beltpp::ilog* plogger_rpc);
    rpc(rpc&& other) noexcept;
    ~rpc();

    void wake();
    bool run();

    static bool verify_storage_order(std::string const& storage_order_token,
                                     std::string& channel_address,
                                     std::string& storage_address,
                                     std::string& file_uri,
                                     std::string& content_unit_uri,
                                     std::string& session_id,
                                     uint64_t& seconds,
                                     std::chrono::system_clock::time_point& tp);

private:
    std::unique_ptr<detail::rpc_internals> m_pimpl;
};

}

