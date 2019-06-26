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
std::vector<std::string> action_owners(BlockchainMessage::StorageUpdate const& storage_update);
std::vector<std::string> action_participants(BlockchainMessage::StorageUpdate const& storage_update);

void action_validate(BlockchainMessage::SignedTransaction const& signed_transaction,
                     BlockchainMessage::StorageUpdate const& storage_update,
                     bool check_complete);

bool action_is_complete(BlockchainMessage::SignedTransaction const& signed_transaction,
                        BlockchainMessage::StorageUpdate const& storage_update);

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      BlockchainMessage::StorageUpdate const& storage_update,
                      state_layer layer);

void action_apply(publiqpp::detail::node_internals& impl,
                  BlockchainMessage::StorageUpdate const& storage_update,
                  state_layer layer);

void action_revert(publiqpp::detail::node_internals& impl,
                   BlockchainMessage::StorageUpdate const& storage_update,
                   state_layer layer);
}
