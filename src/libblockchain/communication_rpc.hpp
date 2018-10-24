#pragma once

#include "node_internals.hpp"

#include "transaction_pool.hpp"

void submit_reward(beltpp::packet&& package,
                   std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                   beltpp::isocket& sk,
                   beltpp::isocket::peer_id const& peerid);

void get_actions(LoggedTransactionsRequest const& msg_get_actions,
                 publiqpp::action_log& action_log,
                 beltpp::isocket& sk,
                 beltpp::isocket::peer_id const& peerid);

void get_hash(beltpp::packet const& packet,
              beltpp::isocket& sk,
              beltpp::isocket::peer_id const& peerid);

void get_random_seed(beltpp::isocket& sk,
                     beltpp::isocket::peer_id const& peerid);

void get_key_pair(beltpp::packet const& packet,
                  beltpp::isocket& sk,
                  beltpp::isocket::peer_id const& peerid);

void get_signature(beltpp::packet const& packet,
                   beltpp::isocket& sk,
                   beltpp::isocket::peer_id const& peerid);

void verify_signature(beltpp::packet const& packet,
                      beltpp::isocket& sk,
                      beltpp::isocket::peer_id const& peerid);

bool process_transfer(BlockchainMessage::SignedTransaction const& signed_transaction,
                      BlockchainMessage::Transfer const& transfer,
                      std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

void process_broadcast(BlockchainMessage::Broadcast&& broadcast,
                       beltpp::isocket::peer_id const& self,
                       beltpp::isocket::peer_id const& from,
                       bool from_rpc,
                       beltpp::ilog* plog,
                       std::unordered_set<beltpp::isocket::peer_id> const& all_peers,
                       beltpp::isocket* psk);


//---------------- Exceptions -----------------------
class authority_exception : public std::runtime_error
{
public:
    explicit authority_exception(std::string const& authority_provided, std::string const& authority_required);

    authority_exception(authority_exception const&) noexcept;
    authority_exception& operator=(authority_exception const&) noexcept;

    virtual ~authority_exception() noexcept;

    std::string authority_provided;
    std::string authority_required;
};

class wrong_request_exception : public std::runtime_error
{
public:
    explicit wrong_request_exception(std::string const& _message);

    wrong_request_exception(wrong_request_exception const&) noexcept;
    wrong_request_exception& operator=(wrong_request_exception const&) noexcept;

    virtual ~wrong_request_exception() noexcept;

    std::string message;
};
