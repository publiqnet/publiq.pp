#pragma once

#include "global.hpp"
#include "message.hpp"
#include "node.hpp"

#include <string>

namespace publiqpp
{
void action_validate(BlockchainMessage::SignedTransaction const& signed_transaction,
                     BlockchainMessage::Role const& role);

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      BlockchainMessage::Role const& role);

void action_apply(publiqpp::detail::node_internals& impl,
                  BlockchainMessage::Role const& role);

void action_revert(publiqpp::detail::node_internals& impl,
                   BlockchainMessage::Role const& role);
}
