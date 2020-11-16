#pragma once

#include "global.hpp"
#include "message.hpp"
#include "node.hpp"
#include "state.hpp"

#include <string>
#include <vector>

namespace publiqpp
{
std::vector<std::string> action_owners(BlockchainMessage::AuthorizationUpdate const& authorization_update);
std::vector<std::string> action_participants(BlockchainMessage::AuthorizationUpdate const& authorization_update);

void action_validate(BlockchainMessage::SignedTransaction const& signed_transaction,
                     BlockchainMessage::AuthorizationUpdate const& authorization_update,
                     bool check_complete);

bool action_is_complete(BlockchainMessage::SignedTransaction const& signed_transaction,
                        BlockchainMessage::AuthorizationUpdate const& authorization_update);

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      BlockchainMessage::SignedTransaction const& signed_transaction,
                      BlockchainMessage::AuthorizationUpdate const& authorization_update,
                      state_layer layer);

void action_apply(publiqpp::detail::node_internals& impl,
                  BlockchainMessage::SignedTransaction const& signed_transaction,
                  BlockchainMessage::AuthorizationUpdate const& authorization_update,
                  state_layer layer);

void action_revert(publiqpp::detail::node_internals& impl,
                   BlockchainMessage::SignedTransaction const& signed_transaction,
                   BlockchainMessage::AuthorizationUpdate const& authorization_update,
                   state_layer layer);
}
