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
// sponsoring stuff
std::vector<std::string> action_owners(BlockchainMessage::SponsorContentUnit const& sponsor_content_unit);
std::vector<std::string> action_participants(BlockchainMessage::SponsorContentUnit const& sponsor_content_unit);

void action_validate(BlockchainMessage::SignedTransaction const& signed_transaction,
                     BlockchainMessage::SponsorContentUnit const& sponsor_content_unit,
                     bool check_complete);

bool action_is_complete(BlockchainMessage::SignedTransaction const& signed_transaction,
                        BlockchainMessage::SponsorContentUnit const& sponsor_content_unit);

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      BlockchainMessage::SignedTransaction const& signed_transaction,
                      BlockchainMessage::SponsorContentUnit const& sponsor_content_unit,
                      state_layer layer);

void action_apply(publiqpp::detail::node_internals& impl,
                  BlockchainMessage::SignedTransaction const& signed_transaction,
                  BlockchainMessage::SponsorContentUnit const& sponsor_content_unit,
                  state_layer layer);

void action_revert(publiqpp::detail::node_internals& impl,
                   BlockchainMessage::SignedTransaction const& signed_transaction,
                   BlockchainMessage::SponsorContentUnit const& sponsor_content_unit,
                   state_layer layer);

// sponsoring stuff Ex
std::vector<std::string> action_owners(BlockchainMessage::SponsorContentUnitEx const& sponsor_content_unit);
std::vector<std::string> action_participants(BlockchainMessage::SponsorContentUnitEx const& sponsor_content_unit);

void action_validate(BlockchainMessage::SignedTransaction const& signed_transaction,
                     BlockchainMessage::SponsorContentUnitEx const& sponsor_content_unit,
                     bool check_complete);

bool action_is_complete(BlockchainMessage::SignedTransaction const& signed_transaction,
                        BlockchainMessage::SponsorContentUnitEx const& sponsor_content_unit);

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      BlockchainMessage::SignedTransaction const& signed_transaction,
                      BlockchainMessage::SponsorContentUnitEx const& sponsor_content_unit,
                      state_layer layer);

void action_apply(publiqpp::detail::node_internals& impl,
                  BlockchainMessage::SignedTransaction const& signed_transaction,
                  BlockchainMessage::SponsorContentUnitEx const& sponsor_content_unit,
                  state_layer layer);

void action_revert(publiqpp::detail::node_internals& impl,
                   BlockchainMessage::SignedTransaction const& signed_transaction,
                   BlockchainMessage::SponsorContentUnitEx const& sponsor_content_unit,
                   state_layer layer);

// cancel sponsoring stuff
std::vector<std::string> action_owners(BlockchainMessage::CancelSponsorContentUnit const& cancel_sponsor_content_unit);
std::vector<std::string> action_participants(BlockchainMessage::CancelSponsorContentUnit const& cancel_sponsor_content_unit);

void action_validate(BlockchainMessage::SignedTransaction const& signed_transaction,
                     BlockchainMessage::CancelSponsorContentUnit const& cancel_sponsor_content_unit,
                     bool check_complete);

bool action_is_complete(BlockchainMessage::SignedTransaction const& signed_transaction,
                        BlockchainMessage::CancelSponsorContentUnit const& cancel_sponsor_content_unit);

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      BlockchainMessage::SignedTransaction const& signed_transaction,
                      BlockchainMessage::CancelSponsorContentUnit const& cancel_sponsor_content_unit,
                      state_layer layer);

void action_apply(publiqpp::detail::node_internals& impl,
                  BlockchainMessage::SignedTransaction const& signed_transaction,
                  BlockchainMessage::CancelSponsorContentUnit const& cancel_sponsor_content_unit,
                  state_layer layer);

void action_revert(publiqpp::detail::node_internals& impl,
                   BlockchainMessage::SignedTransaction const& signed_transaction,
                   BlockchainMessage::CancelSponsorContentUnit const& cancel_sponsor_content_unit,
                   state_layer layer);
}
