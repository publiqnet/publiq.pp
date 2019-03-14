#pragma once

#include "global.hpp"
#include "message.hpp"
#include "node.hpp"

#include <string>

namespace publiqpp
{
void action_validate(BlockchainMessage::SignedTransaction const& signed_transaction,
                     BlockchainMessage::StatInfo const& stat_info);

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      BlockchainMessage::StatInfo const& stat_info);

void action_apply(publiqpp::detail::node_internals& impl,
                  BlockchainMessage::StatInfo const& stat_info);

void action_revert(publiqpp::detail::node_internals& impl,
                   BlockchainMessage::StatInfo const& stat_info);
}
