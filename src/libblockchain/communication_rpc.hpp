#pragma once

#include "action_log.hpp"
#include "blockchain.hpp"
#include "transaction_pool.hpp"
#include "state.hpp"

#include <belt.pp/isocket.hpp>
#include <belt.pp/ilog.hpp>

#include <unordered_set>
#include <exception>

void submit_action(beltpp::packet&& package,
                   publiqpp::action_log& action_log,
                   publiqpp::transaction_pool& transaction_pool,
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
                      publiqpp::action_log& action_log,
                      publiqpp::transaction_pool& transaction_pool,
                      publiqpp::state& state);

void insert_blocks(std::vector<beltpp::packet>& package_blocks,
                   publiqpp::action_log& action_log,
                   publiqpp::transaction_pool& transaction_pool,
                   publiqpp::state& state);

void revert_blocks(size_t count,
                   publiqpp::blockchain& blockchain,
                   publiqpp::transaction_pool& transaction_pool);

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
