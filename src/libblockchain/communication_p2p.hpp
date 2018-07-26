#pragma once

#include "node_internals.hpp"

///////////////////////////////////////////////////////////////////////////////////
//                            Internal Finctions

bool insert_blocks(std::vector<SignedBlock>& signed_block_vector,
                   std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

void revert_blocks(size_t count,
                   std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

///////////////////////////////////////////////////////////////////////////////////

void process_sync_request(beltpp::packet& package,
                          std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                          beltpp::isocket& sk,
                          beltpp::isocket::peer_id const& peerid);

void process_consensus_request(beltpp::packet& package,
                               std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                               beltpp::isocket& sk,
                               beltpp::isocket::peer_id const& peerid);

void process_consensus_response(beltpp::packet& package,
                                std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

void process_blockheader_request(beltpp::packet& package,
                                 std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                                 beltpp::isocket& sk,
                                 beltpp::isocket::peer_id const& peerid);

void process_blockheader_response(beltpp::packet& package,
                                  std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                                  beltpp::isocket& sk,
                                  beltpp::isocket::peer_id const& peerid);

void process_blockchain_request(beltpp::packet& package,
                                std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                                beltpp::isocket& sk,
                                beltpp::isocket::peer_id const& peerid);

void process_blockchain_response(beltpp::packet& package,
                                 std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                                 beltpp::isocket& sk,
                                 beltpp::isocket::peer_id const& peerid);