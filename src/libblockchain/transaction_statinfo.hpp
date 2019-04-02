#pragma once

#include "global.hpp"
#include "message.hpp"
#include "node.hpp"
#include "common.hpp"
#include "state.hpp"

#include <string>

namespace publiqpp
{
void action_validate(BlockchainMessage::SignedTransaction const& signed_transaction,
                     BlockchainMessage::ServiceStatistics const& service_statistics,
                     bool check_complete);

authorization_process_result action_authorization_process(BlockchainMessage::SignedTransaction& signed_transaction,
                                                          BlockchainMessage::ServiceStatistics const& service_statistics);

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      BlockchainMessage::ServiceStatistics const& service_statistics);

void action_apply(publiqpp::detail::node_internals& impl,
                  BlockchainMessage::ServiceStatistics const& service_statistics,
                  state_layer layer);

void action_revert(publiqpp::detail::node_internals& impl,
                   BlockchainMessage::ServiceStatistics const& service_statistics,
                   state_layer layer);
}
