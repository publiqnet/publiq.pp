#pragma once

#include "global.hpp"

#include <belt.pp/ilog.hpp>
#include <belt.pp/isocket.hpp>

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
         beltpp::ilog* plogger_p2p,
         beltpp::ilog* plogger_node);
    node(node&& other);
    ~node();

    std::string name() const;
    bool run();

private:
    std::unique_ptr<detail::node_internals> m_pimpl;
};

}

