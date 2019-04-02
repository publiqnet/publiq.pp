#pragma once

#include "global.hpp"
#include "message.hpp"
#include "node.hpp"
#include "common.hpp"
#include "state.hpp"

#include <string>

namespace publiqpp
{
void action_validate(BlockchainMessage::SignedTransaction const& signed_transaction,
                     BlockchainMessage::StorageUpdate const& storage_update,
                     bool check_complete);

authorization_process_result action_authorization_process(BlockchainMessage::SignedTransaction& signed_transaction,
                                                          BlockchainMessage::StorageUpdate const& storage_update);

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      BlockchainMessage::StorageUpdate const& storage_update);

void action_apply(publiqpp::detail::node_internals& impl,
                  BlockchainMessage::StorageUpdate const& storage_update,
                  state_layer layer);

void action_revert(publiqpp::detail::node_internals& impl,
                   BlockchainMessage::StorageUpdate const& storage_update,
                   state_layer layer);
}
