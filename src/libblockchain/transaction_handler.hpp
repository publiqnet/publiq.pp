#pragma once

#include "node.hpp"
#include "node_internals.hpp"

#include "exception.hpp"

#include <belt.pp/packet.hpp>

#include <string>
#include <chrono>

namespace publiqpp
{
void signed_transaction_validate(SignedTransaction& signed_transaction,
                                 std::chrono::system_clock::time_point const& now,
                                 std::chrono::seconds const& time_shift);

broadcast_type action_process_on_chain(BlockchainMessage::SignedTransaction& signed_transaction,
                                       publiqpp::detail::node_internals& impl);

void action_validate(publiqpp::detail::node_internals& impl,
                     BlockchainMessage::SignedTransaction const& signed_transaction,
                     bool check_complete);

authorization_process_result action_authorization_process(publiqpp::detail::node_internals& impl,
                                                          BlockchainMessage::SignedTransaction& signed_transaction);

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
