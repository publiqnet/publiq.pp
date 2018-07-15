#pragma once

#include "action_log.hpp"
#include "transaction_pool.hpp"
#include "state.hpp"

#include <belt.pp/isocket.hpp>
#include <belt.pp/ilog.hpp>

#include <unordered_set>

void submit_action(beltpp::packet&& package,
                   publiqpp::action_log& action_log,
                   publiqpp::transaction_pool& transaction_pool,
                   beltpp::isocket& sk,
                   beltpp::isocket::peer_id const& peerid);

void revert_last_action(publiqpp::action_log& action_log,
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
                      publiqpp::state& state,
                      beltpp::isocket& sk,
                      beltpp::isocket::peer_id const& peerid);

bool process_block(beltpp::packet const& package_block,
                   publiqpp::action_log& action_log,
                   publiqpp::transaction_pool& transaction_pool,
                   publiqpp::state& state);

void broadcast(beltpp::packet& package_broadcast,
               beltpp::isocket::peer_id const& self,
               beltpp::isocket::peer_id const& from,
               bool from_rpc,
               beltpp::ilog* plog,
               std::unordered_set<beltpp::isocket::peer_id> const& all_peers,
               beltpp::isocket* psk);
