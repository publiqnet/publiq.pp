#pragma once

#include "global.hpp"
#include "message.hpp"
#include "node.hpp"
#include "state.hpp"

#include <string>
#include <vector>

namespace publiqpp
{
std::vector<std::string> action_owners(BlockchainMessage::AwardAuthorization const& award_authorization);
std::vector<std::string> action_participants(BlockchainMessage::AwardAuthorization const& award_authorization);

void action_validate(BlockchainMessage::SignedTransaction const& signed_transaction,
                     BlockchainMessage::AwardAuthorization const& award_authorization,
                     bool check_complete);

bool action_is_complete(BlockchainMessage::SignedTransaction const& signed_transaction,
                        BlockchainMessage::AwardAuthorization const& award_authorization);

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      BlockchainMessage::SignedTransaction const& signed_transaction,
                      BlockchainMessage::AwardAuthorization const& award_authorization,
                      state_layer layer);

void action_apply(publiqpp::detail::node_internals& impl,
                  BlockchainMessage::SignedTransaction const& signed_transaction,
                  BlockchainMessage::AwardAuthorization const& award_authorization,
                  state_layer layer);

void action_revert(publiqpp::detail::node_internals& impl,
                   BlockchainMessage::SignedTransaction const& signed_transaction,
                   BlockchainMessage::AwardAuthorization const& award_authorization,
                   state_layer layer);

std::vector<std::string> action_owners(BlockchainMessage::RejectAuthorization const& reject_authorization);
std::vector<std::string> action_participants(BlockchainMessage::RejectAuthorization const& reject_authorization);

void action_validate(BlockchainMessage::SignedTransaction const& signed_transaction,
                     BlockchainMessage::RejectAuthorization const& reject_authorization,
                     bool check_complete);

bool action_is_complete(BlockchainMessage::SignedTransaction const& signed_transaction,
                        BlockchainMessage::RejectAuthorization const& reject_authorization);

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      BlockchainMessage::SignedTransaction const& signed_transaction,
                      BlockchainMessage::RejectAuthorization const& reject_authorization,
                      state_layer layer);

void action_apply(publiqpp::detail::node_internals& impl,
                  BlockchainMessage::SignedTransaction const& signed_transaction,
                  BlockchainMessage::RejectAuthorization const& reject_authorization,
                  state_layer layer);

void action_revert(publiqpp::detail::node_internals& impl,
                   BlockchainMessage::SignedTransaction const& signed_transaction,
                   BlockchainMessage::RejectAuthorization const& reject_authorization,
                   state_layer layer);
}
