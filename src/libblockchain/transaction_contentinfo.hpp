#pragma once

#include "global.hpp"
#include "message.hpp"
#include "node.hpp"

#include <string>

namespace publiqpp
{
void action_validate(BlockchainMessage::SignedTransaction const& signed_transaction,
                     BlockchainMessage::ContentInfo const& content_info);

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      BlockchainMessage::ContentInfo const& content_info);

void action_apply(publiqpp::detail::node_internals& impl,
                  BlockchainMessage::ContentInfo const& content_info);

void action_revert(publiqpp::detail::node_internals& impl,
                   BlockchainMessage::ContentInfo const& content_info);
}
