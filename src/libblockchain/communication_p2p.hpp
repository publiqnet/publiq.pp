#pragma once

#include "node_internals.hpp"

namespace publiqpp
{
void mine_block(unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

void broadcast_node_type(std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

void broadcast_address_info(std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

void broadcast_storage_stat(StatInfo& stat_info,
                            std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

bool process_address_info(BlockchainMessage::SignedTransaction const& signed_transaction,
                          BlockchainMessage::AddressInfo const& address_info,
                          std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

bool apply_transaction(BlockchainMessage::SignedTransaction const& signed_transaction,
                       publiqpp::detail::node_internals& impl,
                       std::string const& key = std::string());

void revert_transaction(BlockchainMessage::SignedTransaction const& signed_transaction,
                        publiqpp::detail::node_internals& impl,
                        std::string const& key = std::string());

void revert_pool(time_t expiry_time,
                 publiqpp::detail::node_internals& impl,
                 std::multimap<BlockchainMessage::ctime, BlockchainMessage::SignedTransaction>& pool_transactions);

//  this has opposite bool logic - true means error :)
bool check_headers(BlockchainMessage::BlockHeader const& next_header,
                   BlockchainMessage::BlockHeader const& header);
bool check_rewards(BlockchainMessage::Block const& block,
                   std::string const& authority,
                   publiqpp::detail::node_internals& impl);
void broadcast_storage_info(publiqpp::detail::node_internals& impl);
}// end of namespace publiqpp
