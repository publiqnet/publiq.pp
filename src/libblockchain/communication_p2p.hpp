#pragma once

#include "node_internals.hpp"

namespace publiqpp
{
void mine_block(unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

void process_blockheader_request(BlockHeaderRequest const& header_request,
                                 unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                                 beltpp::isocket& sk,
                                 beltpp::isocket::peer_id const& peerid);

void process_blockheader_response(BlockHeaderResponse&& header_response,
                                  unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                                  beltpp::isocket& sk,
                                  beltpp::isocket::peer_id const& peerid);

void process_blockchain_request(BlockchainRequest const& blockchain_request,
                                unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                                beltpp::isocket& sk,
                                beltpp::isocket::peer_id const& peerid);

void process_blockchain_response(BlockchainResponse&& blockchain_response,
                                 std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                                 beltpp::isocket& sk,
                                 beltpp::isocket::peer_id const& peerid);

void broadcast_node_type(std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

void broadcast_address_info(std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

void broadcast_storage_stat(StatInfo& stat_info,
                            std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

bool process_role(BlockchainMessage::SignedTransaction const& signed_transaction,
                  BlockchainMessage::Role const& role,
                  std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

bool process_stat_info(BlockchainMessage::SignedTransaction const& signed_transaction,
                       BlockchainMessage::StatInfo const& stat_info,
                       std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

bool process_address_info(BlockchainMessage::SignedTransaction const& signed_transaction,
                          BlockchainMessage::AddressInfo const& address_info,
                          std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);
}// end of namespace publiqpp
