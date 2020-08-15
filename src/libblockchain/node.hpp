#pragma once

#include "global.hpp"
#include "message.hpp"
#include "config.hpp"

#include <belt.pp/ilog.hpp>
#include <belt.pp/ievent.hpp>
#include <belt.pp/isocket.hpp>
#include <belt.pp/direct_stream.hpp>

#include <mesh.pp/cryptoutility.hpp>

#include <boost/filesystem/path.hpp>

#include <memory>
#include <string>
#include <chrono>
#include <map>
#include <vector>

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

using fp_content_unit_validate_check =
bool (*)(std::vector<std::string> const& content_unit_file_uris,
std::string& find_duplicate,
uint64_t block_number,
bool is_testnet);

    class node_internals;
}

class BLOCKCHAINSHARED_EXPORT node
{
public:
    node(std::string const& genesis_signed_block,
         boost::filesystem::path const& fs_blockchain,
         boost::filesystem::path const& fs_action_log,
         boost::filesystem::path const& fs_transaction_pool,
         boost::filesystem::path const& fs_state,
         boost::filesystem::path const& fs_documents,
         boost::filesystem::path const& fs_storages,
         boost::filesystem::path const& fs_storage,
         boost::filesystem::path const& fs_inbox,
         beltpp::ilog* plogger_p2p,
         beltpp::ilog* plogger_node,
         config& ref_config,
         uint64_t freeze_before_block,
         uint64_t revert_blocks_count,
         uint64_t revert_actions_count,
         bool resync,
         coin const& mine_amount_threshhold,
         std::vector<coin> const& block_reward_array,
         detail::fp_counts_per_channel_views p_counts_per_channel_views,
         detail::fp_content_unit_validate_check p_content_unit_validate_check,
         beltpp::direct_channel& channel,
         std::unique_ptr<beltpp::event_handler>&& inject_eh = nullptr,
         std::unique_ptr<beltpp::socket>&& inject_rpc_socket = nullptr,
         std::unique_ptr<beltpp::socket>&& inject_p2p_socket = nullptr);
    node(node&& other) noexcept;
    ~node();

    void wake();
    std::string name() const;
    bool run(bool& stop);

private:
    std::unique_ptr<detail::node_internals> m_pimpl;
};

}

