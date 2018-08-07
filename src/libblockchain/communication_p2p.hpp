#pragma once

#include "node_internals.hpp"

///////////////////////////////////////////////////////////////////////////////////
//                            Internal Finctions
//uint64_t calc_delta(string const& key, uint64_t amount, BlockHeader const& block_header);
//
//bool insert_blocks(vector<SignedBlock>& signed_block_vector,
//                   unique_ptr<publiqpp::detail::node_internals>& m_pimpl);
//
//void revert_blocks(size_t count,
//                   unique_ptr<publiqpp::detail::node_internals>& m_pimpl);
//
///////////////////////////////////////////////////////////////////////////////////

void insert_genesis(std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

void mine_block(unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

void process_sync_request(beltpp::packet& package,
                          unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                          beltpp::isocket& sk,
                          beltpp::isocket::peer_id const& peerid);

void process_blockheader_request(beltpp::packet& package,
                                 unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                                 beltpp::isocket& sk,
                                 beltpp::isocket::peer_id const& peerid);

void process_blockheader_response(beltpp::packet& package,
                                  unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                                  beltpp::isocket& sk,
                                  beltpp::isocket::peer_id const& peerid);

void process_blockchain_request(beltpp::packet& package,
                                unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                                beltpp::isocket& sk,
                                beltpp::isocket::peer_id const& peerid);

void process_blockchain_response(beltpp::packet& package,
                                 std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                                 beltpp::isocket& sk,
                                 beltpp::isocket::peer_id const& peerid);

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
