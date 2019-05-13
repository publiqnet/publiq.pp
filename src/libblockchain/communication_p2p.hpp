#pragma once

#include "node_internals.hpp"

#include <map>

namespace publiqpp
{
void mine_block(publiqpp::detail::node_internals& impl);

void broadcast_node_type(std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

void broadcast_address_info(std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

bool process_address_info(BlockchainMessage::SignedTransaction const& signed_transaction,
                          BlockchainMessage::AddressInfo const& address_info,
                          std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

bool apply_transaction(BlockchainMessage::SignedTransaction const& signed_transaction,
                       publiqpp::detail::node_internals& impl,
                       std::string const& key = std::string());

void revert_transaction(BlockchainMessage::SignedTransaction const& signed_transaction,
                        publiqpp::detail::node_internals& impl,
                        std::string const& key = std::string());

std::multimap<BlockchainMessage::ctime, BlockchainMessage::SignedTransaction>
revert_pool(time_t expiry_time, publiqpp::detail::node_internals& impl);

//  this has opposite bool logic - true means error :)
bool check_headers(BlockchainMessage::BlockHeaderExtended const& next_header,
                   BlockchainMessage::BlockHeaderExtended const& header);

bool check_rewards(BlockchainMessage::Block const& block,
                   std::string const& authority,
                   publiqpp::detail::node_internals& impl);

uint64_t check_delta_vector(vector<pair<uint64_t, uint64_t>> const& delta_vector, std::string& error);
}// end of namespace publiqpp
