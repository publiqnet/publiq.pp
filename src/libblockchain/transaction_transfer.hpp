#pragma once

#include "global.hpp"
#include "message.hpp"
#include "node.hpp"

#include <string>

namespace publiqpp
{
void action_validate(BlockchainMessage::SignedTransaction const& signed_transaction,
                     BlockchainMessage::Transfer const& transfer);

bool action_can_apply(std::unique_ptr<publiqpp::detail::node_internals> const& pimpl,
                      BlockchainMessage::Transfer const& transfer);

void action_apply(std::unique_ptr<publiqpp::detail::node_internals>& pimpl,
                  BlockchainMessage::Transfer const& transfer);

void action_revert(std::unique_ptr<publiqpp::detail::node_internals>& pimpl,
                   BlockchainMessage::Transfer const& transfer);
}
