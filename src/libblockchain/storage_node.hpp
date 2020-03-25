#pragma once

#include "global.hpp"

#include <belt.pp/ilog.hpp>
#include <belt.pp/isocket.hpp>

#include <mesh.pp/cryptoutility.hpp>

#include <boost/filesystem/path.hpp>

#include <memory>

namespace publiqpp
{
class node;
namespace detail
{
    class storage_node_internals;
}

class BLOCKCHAINSHARED_EXPORT storage_node
{
    friend class node;
public:
    storage_node(node& master_node,
                 beltpp::ip_address const& rpc_bind_to_address,
                 boost::filesystem::path const& fs_storage,
                 meshpp::private_key const& pv_key,
                 beltpp::ilog* plogger_storage_node);
    storage_node(storage_node&& other) noexcept;
    ~storage_node();

    void wake();
    void run(bool& stop);

    beltpp::stream::packets receive();
    void send(beltpp::packet&& pack);

private:
    std::unique_ptr<detail::storage_node_internals> m_pimpl;
};

}

