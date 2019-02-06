#pragma once

#include "global.hpp"

#include <belt.pp/ilog.hpp>
#include <belt.pp/isocket.hpp>

#include <mesh.pp/cryptoutility.hpp>

#include <boost/filesystem/path.hpp>

#include <memory>

namespace publiqpp
{
namespace detail
{
    class storage_node_internals;
}

class BLOCKCHAINSHARED_EXPORT storage_node
{
public:
    storage_node(beltpp::ip_address const& rpc_bind_to_address,
                 boost::filesystem::path const& fs_storage,
                 meshpp::public_key parent_pb_key,
                 beltpp::ilog* plogger_storage_node);
    storage_node(storage_node&& other) noexcept;
    ~storage_node();

    void terminate();
    bool run();

private:
    std::unique_ptr<detail::storage_node_internals> m_pimpl;
};

}

