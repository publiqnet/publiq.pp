#pragma once

#include "state.hpp"

#include <belt.pp/isocket.hpp>

void submit_actions(beltpp::packet const& packet,
                    publiqpp::state& state,
                    beltpp::isocket& sk,
                    beltpp::isocket::peer_id const& peerid);

void get_actions(beltpp::packet const& packet,
                 publiqpp::state& state,
                 beltpp::isocket& sk,
                 beltpp::isocket::peer_id const& peerid);

void get_hash(beltpp::packet const& packet,
              publiqpp::state& state,
              beltpp::isocket& sk,
              beltpp::isocket::peer_id const& peerid);
