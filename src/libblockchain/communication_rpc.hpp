#pragma once

#include "action_log.hpp"
#include "transaction_pool.hpp"

#include <belt.pp/isocket.hpp>

void submit_actions(beltpp::packet const& packet,
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
