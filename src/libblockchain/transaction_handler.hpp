#pragma once

#include "node.hpp"
#include "node_internals.hpp"

#include "transaction_transfer.hpp"
#include "transaction_file.hpp"

#include "exception.hpp"

#include <belt.pp/packet.hpp>

#include <string>

namespace publiqpp
{
bool action_process(BlockchainMessage::SignedTransaction const& signed_transaction,
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

template <typename T_action>
bool action_process(BlockchainMessage::SignedTransaction const& signed_transaction,
                    T_action const& action,
                    std::unique_ptr<publiqpp::detail::node_internals>& pimpl)
{
    action_validate(signed_transaction, action);

    // Don't need to store transaction if sync in process
    // and seems is too far from current block.
    // Just will check the transaction and broadcast
    if (pimpl->sync_headers.size() > BLOCK_TR_LENGTH)
        return true;

    // Check pool
    std::string tr_hash = meshpp::hash(signed_transaction.to_string());

    if (pimpl->m_transaction_cache.count(tr_hash))
        return false;

    auto transaction_cache_backup = pimpl->m_transaction_cache;

    beltpp::on_failure guard([&pimpl, &transaction_cache_backup]
    {
        pimpl->discard();
        pimpl->m_transaction_cache = std::move(transaction_cache_backup);
    });

    // Validate and add to state
    action_apply(pimpl, action);

    fee_validate(pimpl, signed_transaction);

    // Add to the pool
    pimpl->m_transaction_pool.push_back(signed_transaction);
    pimpl->m_transaction_cache[tr_hash] =
        system_clock::from_time_t(signed_transaction.transaction_details.creation.tm);

    // Add to action log
    pimpl->m_action_log.log_transaction(signed_transaction);

    pimpl->save(guard);

    return true;
}
}
