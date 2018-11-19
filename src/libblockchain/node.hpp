#pragma once

#include "global.hpp"

#include <belt.pp/ilog.hpp>
#include <belt.pp/isocket.hpp>

#include <mesh.pp/cryptoutility.hpp>

#include <boost/filesystem/path.hpp>

#include <memory>

namespace publiqpp
{
    enum class node_type{ unknown = 0, miner = 1, channel = 2, storage = 3 };

namespace detail
{
    class node_internals;

    inline
    uint64_t node_type_to_int(node_type input)
    {
        switch (input)
        {
        case node_type::miner: return 1;
        case node_type::channel: return 2;
        case node_type::storage: return 3;
        }

        return 0;
    }

    inline
    node_type int_to_node_type(uint64_t input)
    {
        switch (input)
        {
        case 1: return node_type::miner;
        case 2: return node_type::channel;
        case 3: return node_type::storage;
        }

        return node_type::unknown;
    }
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

