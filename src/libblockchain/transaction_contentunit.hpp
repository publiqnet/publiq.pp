#pragma once

#include "global.hpp"
#include "message.hpp"
#include "node.hpp"

#include <string>

namespace publiqpp
{
void action_validate(BlockchainMessage::SignedTransaction const& signed_transaction,
                     BlockchainMessage::ContentUnit const& content_unit);

bool action_can_apply(std::unique_ptr<publiqpp::detail::node_internals> const& pimpl,
                      BlockchainMessage::ContentUnit const& content_unit);

void action_apply(std::unique_ptr<publiqpp::detail::node_internals>& pimpl,
                  BlockchainMessage::ContentUnit const& content_unit);

void action_revert(std::unique_ptr<publiqpp::detail::node_internals>& pimpl,
                   BlockchainMessage::ContentUnit const& content_unit);
}
