#pragma once

#include "node_internals.hpp"

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

void broadcast_article_info(StorageFileAddress file_address,
                            std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

void broadcast_content_info(StorageFileAddress file_address,
                            std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

bool process_contract(BlockchainMessage::SignedTransaction const& signed_transaction,
                      BlockchainMessage::Contract const& contract,
                      std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

bool process_stat_info(BlockchainMessage::SignedTransaction const& signed_transaction,
                       BlockchainMessage::StatInfo const& stat_info,
                       std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

bool process_article_info(BlockchainMessage::SignedTransaction const& signed_transaction,
                          BlockchainMessage::ArticleInfo const& article_info,
                          std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

bool process_content_info(BlockchainMessage::SignedTransaction const& signed_transaction,
                          BlockchainMessage::ContentInfo const& content_info,
                          std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

bool process_address_info(BlockchainMessage::SignedTransaction const& signed_transaction,
                          BlockchainMessage::AddressInfo const& address_info,
                          std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

//---------------- Exceptions -----------------------
class wrong_data_exception : public std::runtime_error
{
public:
    explicit wrong_data_exception(std::string const& _message);

    wrong_data_exception(wrong_data_exception const&) noexcept;
    wrong_data_exception& operator=(wrong_data_exception const&) noexcept;

    virtual ~wrong_data_exception() noexcept;

    std::string message;
};
