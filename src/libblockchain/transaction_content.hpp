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
                     BlockchainMessage::Content const& content,
                     bool check_complete);

authorization_process_result action_authorization_process(BlockchainMessage::SignedTransaction& signed_transaction,
                                                          BlockchainMessage::Content const& content);

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      BlockchainMessage::Content const& content);

void action_apply(publiqpp::detail::node_internals& impl,
                  BlockchainMessage::Content const& content,
                  state_layer layer);

void action_revert(publiqpp::detail::node_internals& impl,
                   BlockchainMessage::Content const& content,
                   state_layer layer);
}
