#pragma once

#include "node.hpp"
#include "node_internals.hpp"

#include "exception.hpp"

#include <belt.pp/packet.hpp>

#include <string>

namespace publiqpp
{
bool action_process_on_chain(BlockchainMessage::SignedTransaction const& signed_transaction,
                             std::unique_ptr<publiqpp::detail::node_internals>& pimpl);

void action_validate(std::unique_ptr<publiqpp::detail::node_internals>& pimpl,
                     BlockchainMessage::SignedTransaction const& signed_transaction);

bool action_can_apply(std::unique_ptr<publiqpp::detail::node_internals> const& pimpl,
                      beltpp::packet const& package);

void action_apply(std::unique_ptr<publiqpp::detail::node_internals>& pimpl,
                  beltpp::packet const& package);

void action_revert(std::unique_ptr<publiqpp::detail::node_internals>& pimpl,
                   beltpp::packet const& package);

void fee_validate(std::unique_ptr<publiqpp::detail::node_internals> const& pimpl,
                  BlockchainMessage::SignedTransaction const& signed_transaction);

bool fee_can_apply(std::unique_ptr<publiqpp::detail::node_internals> const& pimpl,
                   BlockchainMessage::SignedTransaction const& signed_transaction);

void fee_apply(std::unique_ptr<publiqpp::detail::node_internals>& pimpl,
               BlockchainMessage::SignedTransaction const& signed_transaction,
               std::string const& fee_receiver);

void fee_revert(std::unique_ptr<publiqpp::detail::node_internals>& pimpl,
                BlockchainMessage::SignedTransaction const& signed_transaction,
                std::string const& fee_receiver);
}
