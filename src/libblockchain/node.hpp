#pragma once

#include "global.hpp"
#include "message.hpp"

#include <belt.pp/ilog.hpp>
#include <belt.pp/isocket.hpp>

#include <mesh.pp/cryptoutility.hpp>

#include <boost/filesystem/path.hpp>

#include <memory>
#include <string>
#include <chrono>
#include <map>

namespace publiqpp
{
class storage_node;
class coin;
namespace detail
{
using fp_counts_per_channel_views =
uint64_t (*)(std::map<uint64_t, std::map<std::string, std::map<std::string, uint64_t>>> const& item_per_owner,
uint64_t block_number,
bool is_testnet);

    class node_internals;
}

class BLOCKCHAINSHARED_EXPORT node
{
public:
    node(std::string const& genesis_signed_block,
         beltpp::ip_address const& public_address,
         beltpp::ip_address const& public_ssl_address,
         beltpp::ip_address const& rpc_bind_to_address,
         beltpp::ip_address const& p2p_bind_to_address,
         std::vector<beltpp::ip_address> const& p2p_connect_to_addresses,
         boost::filesystem::path const& fs_blockchain,
         boost::filesystem::path const& fs_action_log,
         boost::filesystem::path const& fs_transaction_pool,
         boost::filesystem::path const& fs_state,
         boost::filesystem::path const& fs_documents,
         boost::filesystem::path const& fs_storages,
         boost::filesystem::path const& fs_storage,
         beltpp::ilog* plogger_p2p,
         beltpp::ilog* plogger_node,
         meshpp::private_key const& pv_key,
         BlockchainMessage::NodeType const& n_type,
         uint64_t fractions,
         uint64_t freeze_before_block,
         std::string const& manager_address,
         bool log_enabled,
         bool transfer_only,
         bool testnet,
         bool resync,
         uint64_t revert_blocks_count,
         coin const& mine_amount_threshhold,
         std::vector<coin> const& block_reward_array,
         detail::fp_counts_per_channel_views p_counts_per_channel_views);
    node(node&& other) noexcept;
    ~node();

    void wake();
    std::string name() const;
    void run(bool& stop);
    void set_slave_node(storage_node& slave_node);

private:
    std::unique_ptr<detail::node_internals> m_pimpl;
};

}

