#pragma once

#include "global.hpp"
#include "message.hpp"
#include "node.hpp"
#include "common.hpp"
#include "state.hpp"

#include <string>
#include <vector>

namespace publiqpp
{
std::vector<std::string> action_owners(BlockchainMessage::SponsorContentUnit const& sponsor_content_unit);
std::vector<std::string> action_participants(BlockchainMessage::SponsorContentUnit const& sponsor_content_unit);

void action_validate(BlockchainMessage::SignedTransaction const& signed_transaction,
                     BlockchainMessage::SponsorContentUnit const& sponsor_content_unit,
                     bool check_complete);

bool action_is_complete(BlockchainMessage::SignedTransaction const& signed_transaction,
                        BlockchainMessage::SponsorContentUnit const& sponsor_content_unit);

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      BlockchainMessage::SponsorContentUnit const& sponsor_content_unit,
                      state_layer layer);

void action_apply(publiqpp::detail::node_internals& impl,
                  BlockchainMessage::SponsorContentUnit const& sponsor_content_unit,
                  state_layer layer);

void action_revert(publiqpp::detail::node_internals& impl,
                   BlockchainMessage::SponsorContentUnit const& sponsor_content_unit,
                   state_layer layer);
}
