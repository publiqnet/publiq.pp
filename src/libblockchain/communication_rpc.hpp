#pragma once

#include "node_internals.hpp"

#include "transaction_pool.hpp"

void submit_reward(beltpp::packet&& package,
                   std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl,
                   beltpp::isocket& sk,
                   beltpp::isocket::peer_id const& peerid);

void get_actions(beltpp::packet const& packet,
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

void process_transfer(beltpp::packet const& package_signed_transaction,
                      beltpp::packet const& package_transfer,
                      std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

void broadcast(beltpp::packet& package_broadcast,
               beltpp::isocket::peer_id const& self,
               beltpp::isocket::peer_id const& from,
               bool from_rpc,
               beltpp::ilog* plog,
               std::unordered_set<beltpp::isocket::peer_id> const& all_peers,
               beltpp::isocket* psk);


//---------------- Exceptions -----------------------
class exception_authority : public std::runtime_error
{
public:
    explicit exception_authority(std::string const& authority_provided, std::string const& authority_required);

    exception_authority(exception_authority const&) noexcept;
    exception_authority& operator=(exception_authority const&) noexcept;

    virtual ~exception_authority() noexcept;

    std::string authority_provided;
    std::string authority_required;
};

class exception_balance : public std::runtime_error
{
public:
    explicit exception_balance(std::string const& account);

    exception_balance(exception_balance const&) noexcept;
    exception_balance& operator=(exception_balance const&) noexcept;

    virtual ~exception_balance() noexcept;

    std::string account;
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
