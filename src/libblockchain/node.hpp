#pragma once

#include "global.hpp"
#include "node_internals.hpp"

#include <belt.pp/ilog.hpp>
#include <belt.pp/isocket.hpp>

#include <mesh.pp/cryptoutility.hpp>

#include <boost/filesystem/path.hpp>

#include <memory>

namespace publiqpp
{
namespace detail
{
    class node_internals;
}

class BLOCKCHAINSHARED_EXPORT node
{
public:
    node(beltpp::ip_address const& rpc_bind_to_address,
         beltpp::ip_address const& p2p_bind_to_address,
         std::vector<beltpp::ip_address> const& p2p_connect_to_addresses,
         boost::filesystem::path const& fs_blockchain,
         boost::filesystem::path const& fs_action_log,
         boost::filesystem::path const& fs_storage,
         boost::filesystem::path const& fs_transaction_pool,
         boost::filesystem::path const& fs_state,
         beltpp::ilog* plogger_p2p,
         beltpp::ilog* plogger_node,
         meshpp::private_key const& pv_key,
         node_type& n_type,
         bool log_enabled);
    node(node&& other) noexcept;
    ~node();

    void terminate();
    std::string name() const;
    bool run();

private:
    std::unique_ptr<detail::node_internals> m_pimpl;
};

}

