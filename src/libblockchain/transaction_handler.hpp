#pragma once

#include "node.hpp"
#include "node_internals.hpp"

#include "exception.hpp"

#include <belt.pp/packet.hpp>

#include <string>

namespace publiqpp
{
bool action_process_on_chain(BlockchainMessage::SignedTransaction const& signed_transaction,
                             publiqpp::detail::node_internals& impl);

void action_validate(publiqpp::detail::node_internals& impl,
                     BlockchainMessage::SignedTransaction const& signed_transaction);

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      beltpp::packet const& package);

void action_apply(publiqpp::detail::node_internals& impl,
                  beltpp::packet const& package);

void action_revert(publiqpp::detail::node_internals& impl,
                   beltpp::packet const& package);

void fee_validate(publiqpp::detail::node_internals const& impl,
                  BlockchainMessage::SignedTransaction const& signed_transaction);

bool fee_can_apply(publiqpp::detail::node_internals const& impl,
                   BlockchainMessage::SignedTransaction const& signed_transaction);

void fee_apply(publiqpp::detail::node_internals& impl,
               BlockchainMessage::SignedTransaction const& signed_transaction,
               std::string const& fee_receiver);

void fee_revert(publiqpp::detail::node_internals& impl,
                BlockchainMessage::SignedTransaction const& signed_transaction,
                std::string const& fee_receiver);
}
