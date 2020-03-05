#pragma once

#include "node_internals.hpp"

#include <map>
#include <string>

namespace publiqpp
{
void mine_block(publiqpp::detail::node_internals& impl);

void broadcast_node_type(std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

void broadcast_address_info(std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

bool process_address_info(BlockchainMessage::SignedTransaction const& signed_tx,
                          BlockchainMessage::AddressInfo const& address_info,
                          std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

bool process_update_command(BlockchainMessage::SignedTransaction const& signed_tx,
                            BlockchainMessage::StorageUpdateCommand const& update_command,
                            std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

bool process_black_box(BlockchainMessage::SignedBlackBox const& signed_black_box,
                       std::unique_ptr<publiqpp::detail::node_internals>& pimpl);

void save_black_box(BlackBox const& black_box,
                    std::unique_ptr<publiqpp::detail::node_internals>& pimpl);

void broadcast_service_statistics(publiqpp::detail::node_internals& impl);

void broadcast_storage_update(publiqpp::detail::node_internals& impl,
                              std::string const& uri,
                              BlockchainMessage::UpdateType const& status);

void delete_storage_file(publiqpp::detail::node_internals& impl,
                         beltpp::isocket* psk,
                         std::string const& peerid,
                         std::string const& uri);

bool apply_transaction(BlockchainMessage::SignedTransaction const& signed_transaction,
                       publiqpp::detail::node_internals& impl,
                       std::string const& key = std::string());

void revert_transaction(BlockchainMessage::SignedTransaction const& signed_transaction,
                        publiqpp::detail::node_internals& impl,
                        std::string const& key = std::string());

std::vector<BlockchainMessage::SignedTransaction>
revert_pool(time_t expiry_time, publiqpp::detail::node_internals& impl);

//  this has opposite bool logic - true means error :)
bool check_headers(BlockchainMessage::BlockHeaderExtended const& next_header,
                   BlockchainMessage::BlockHeaderExtended const& header);

enum class rewards_type {apply, revert};
bool check_rewards(BlockchainMessage::Block const& block,
                   std::string const& authority,
                   rewards_type type,
                   publiqpp::detail::node_internals& impl,
                   std::map<std::string, std::map<std::string, uint64_t>>& unit_uri_view_counts,
                   std::map<std::string, coin>& applied_sponsor_items);

bool check_service_statistics(BlockchainMessage::Block const& block,
                              vector<BlockchainMessage::SignedTransaction> const& pool_transactions,
                              vector<BlockchainMessage::SignedTransaction> const& reverted_transactions,
                              publiqpp::detail::node_internals& impl);

uint64_t check_delta_vector(vector<pair<uint64_t, uint64_t>> const& delta_vector, std::string& error);
}// end of namespace publiqpp
