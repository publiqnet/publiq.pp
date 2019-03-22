#pragma once

#include "global.hpp"
#include "message.hpp"
#include "node.hpp"
#include "common.hpp"

#include <string>

namespace publiqpp
{
void action_validate(BlockchainMessage::SignedTransaction const& signed_transaction,
                     BlockchainMessage::Transfer const& transfer,
                     bool check_complete);

authorization_process_result action_authorization_process(BlockchainMessage::SignedTransaction& signed_transaction,
                                                          BlockchainMessage::Transfer const& transfer);

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      BlockchainMessage::Transfer const& transfer);

void action_apply(publiqpp::detail::node_internals& impl,
                  BlockchainMessage::Transfer const& transfer);

void action_revert(publiqpp::detail::node_internals& impl,
                   BlockchainMessage::Transfer const& transfer);
}
