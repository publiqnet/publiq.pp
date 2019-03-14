#pragma once

#include "global.hpp"
#include "message.hpp"
#include "node.hpp"

#include <string>

namespace publiqpp
{
void action_validate(BlockchainMessage::SignedTransaction const& signed_transaction,
                     BlockchainMessage::ContentUnit const& content_unit);

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      BlockchainMessage::ContentUnit const& content_unit);

void action_apply(publiqpp::detail::node_internals& impl,
                  BlockchainMessage::ContentUnit const& content_unit);

void action_revert(publiqpp::detail::node_internals& impl,
                   BlockchainMessage::ContentUnit const& content_unit);
}
