#include "global.hpp"
#include "transaction_handler.hpp"

#include "transaction_transfer.hpp"
#include "transaction_file.hpp"
#include "transaction_contentunit.hpp"
#include "transaction_content.hpp"
#include "transaction_role.hpp"
#include "transaction_contentinfo.hpp"
#include "transaction_statinfo.hpp"

using namespace BlockchainMessage;

namespace publiqpp
{
template <typename T_action>
bool action_process_on_chain_t(BlockchainMessage::SignedTransaction const& signed_transaction,
                               T_action const& action,
                               publiqpp::detail::node_internals& impl);

bool action_process_on_chain(SignedTransaction const& signed_transaction,
                             publiqpp::detail::node_internals& impl)
{
    bool code;
    auto const& package = signed_transaction.transaction_details.action;

    if (impl.m_transfer_only && package.type() != Transfer::rtt)
        throw std::runtime_error("this is coin only blockchain");

    switch (package.type())
    {
    case Transfer::rtt:
    {
        Transfer const* paction;
        package.get(paction);
        code = action_process_on_chain_t(signed_transaction, *paction, impl);
        break;
    }
    case File::rtt:
    {
        File const* paction;
        package.get(paction);
        code = action_process_on_chain_t(signed_transaction, *paction, impl);
        break;
    }
    case ContentUnit::rtt:
    {
        ContentUnit const* paction;
        package.get(paction);
        code = action_process_on_chain_t(signed_transaction, *paction, impl);
        break;
    }
    case Content::rtt:
    {
        Content const* paction;
        package.get(paction);
        code = action_process_on_chain_t(signed_transaction, *paction, impl);
        break;
    }
    case Role::rtt:
    {
        Role const* paction;
        package.get(paction);
        code = action_process_on_chain_t(signed_transaction, *paction, impl);
        break;
    }
    case ContentInfo::rtt:
    {
        ContentInfo const* paction;
        package.get(paction);
        code = action_process_on_chain_t(signed_transaction, *paction, impl);
        break;
    }
    case StatInfo::rtt:
    {
        StatInfo const* paction;
        package.get(paction);
        code = action_process_on_chain_t(signed_transaction, *paction, impl);
        break;
    }
    default:
        throw wrong_data_exception("unknown transaction action type!");
    }

    return code;
}

void action_validate(publiqpp::detail::node_internals& impl,
                     BlockchainMessage::SignedTransaction const& signed_transaction)
{
    auto const& package = signed_transaction.transaction_details.action;

    if (impl.m_transfer_only && package.type() != Transfer::rtt)
        throw std::runtime_error("this is coin only blockchain");

    switch (package.type())
    {
    case Transfer::rtt:
    {
        Transfer const* paction;
        package.get(paction);
        action_validate(signed_transaction, *paction);
        break;
    }
    case File::rtt:
    {
        File const* paction;
        package.get(paction);
        action_validate(signed_transaction, *paction);
        break;
    }
    case ContentUnit::rtt:
    {
        ContentUnit const* paction;
        package.get(paction);
        action_validate(signed_transaction, *paction);
        break;
    }
    case Content::rtt:
    {
        Content const* paction;
        package.get(paction);
        action_validate(signed_transaction, *paction);
        break;
    }
    case Role::rtt:
    {
        Role const* paction;
        package.get(paction);
        action_validate(signed_transaction, *paction);
        break;
    }
    case ContentInfo::rtt:
    {
        ContentInfo const* paction;
        package.get(paction);
        action_validate(signed_transaction, *paction);
        break;
    }
    case StatInfo::rtt:
    {
        StatInfo const* paction;
        package.get(paction);
        action_validate(signed_transaction, *paction);
        break;
    }
    default:
        throw wrong_data_exception("unknown transaction action type!");
    }
}

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      beltpp::packet const& package)
{
    if (impl.m_transfer_only &&
        package.type() != Transfer::rtt)
        throw std::runtime_error("this is coin only blockchain");

    bool code;
    switch (package.type())
    {
    case Transfer::rtt:
    {
        Transfer const* paction;
        package.get(paction);
        code = action_can_apply(impl, *paction);
        break;
    }
    case File::rtt:
    {
        File const* paction;
        package.get(paction);
        code = action_can_apply(impl, *paction);
        break;
    }
    case ContentUnit::rtt:
    {
        ContentUnit const* paction;
        package.get(paction);
        code = action_can_apply(impl, *paction);
        break;
    }
    case Content::rtt:
    {
        Content const* paction;
        package.get(paction);
        code = action_can_apply(impl, *paction);
        break;
    }
    case Role::rtt:
    {
        Role const* paction;
        package.get(paction);
        code = action_can_apply(impl, *paction);
        break;
    }
    case ContentInfo::rtt:
    {
        ContentInfo const* paction;
        package.get(paction);
        code = action_can_apply(impl, *paction);
        break;
    }
    case StatInfo::rtt:
    {
        StatInfo const* paction;
        package.get(paction);
        code = action_can_apply(impl, *paction);
        break;
    }
    default:
        throw wrong_data_exception("unknown transaction action type!");
    }

    return code;
}

void action_apply(publiqpp::detail::node_internals& impl,
                  beltpp::packet const& package)
{
    if (impl.m_transfer_only &&
        package.type() != Transfer::rtt)
        throw std::runtime_error("this is coin only blockchain");

    switch (package.type())
    {
    case Transfer::rtt:
    {
        Transfer const* paction;
        package.get(paction);
        action_apply(impl, *paction);
        break;
    }
    case File::rtt:
    {
        File const* paction;
        package.get(paction);
        action_apply(impl, *paction);
        break;
    }
    case ContentUnit::rtt:
    {
        ContentUnit const* paction;
        package.get(paction);
        action_apply(impl, *paction);
        break;
    }
    case Content::rtt:
    {
        Content const* paction;
        package.get(paction);
        action_apply(impl, *paction);
        break;
    }
    case Role::rtt:
    {
        Role const* paction;
        package.get(paction);
        action_apply(impl, *paction);
        break;
    }
    case ContentInfo::rtt:
    {
        ContentInfo const* paction;
        package.get(paction);
        action_apply(impl, *paction);
        break;
    }
    case StatInfo::rtt:
    {
        StatInfo const* paction;
        package.get(paction);
        action_apply(impl, *paction);
        break;
    }
    default:
        throw wrong_data_exception("unknown transaction action type!");
    }
}

void action_revert(publiqpp::detail::node_internals& impl,
                   beltpp::packet const& package)
{
    if (impl.m_transfer_only &&
        package.type() != Transfer::rtt)
        throw std::runtime_error("this is coin only blockchain");

    switch (package.type())
    {
    case Transfer::rtt:
    {
        Transfer const* paction;
        package.get(paction);
        action_revert(impl, *paction);
        break;
    }
    case File::rtt:
    {
        File const* paction;
        package.get(paction);
        action_revert(impl, *paction);
        break;
    }
    case ContentUnit::rtt:
    {
        ContentUnit const* paction;
        package.get(paction);
        action_revert(impl, *paction);
        break;
    }
    case Content::rtt:
    {
        Content const* paction;
        package.get(paction);
        action_revert(impl, *paction);
        break;
    }
    case Role::rtt:
    {
        Role const* paction;
        package.get(paction);
        action_revert(impl, *paction);
        break;
    }
    case ContentInfo::rtt:
    {
        ContentInfo const* paction;
        package.get(paction);
        action_revert(impl, *paction);
        break;
    }
    case StatInfo::rtt:
    {
        StatInfo const* paction;
        package.get(paction);
        action_revert(impl, *paction);
        break;
    }
    default:
        throw wrong_data_exception("unknown transaction action type!");
    }
}

void fee_validate(publiqpp::detail::node_internals const& impl,
                  BlockchainMessage::SignedTransaction const& signed_transaction)
{
    Coin balance = impl.m_state.get_balance(signed_transaction.authority);
    if (coin(balance) < signed_transaction.transaction_details.fee)
        throw not_enough_balance_exception(coin(balance),
                                           signed_transaction.transaction_details.fee);
}

bool fee_can_apply(publiqpp::detail::node_internals const& impl,
                   SignedTransaction const& signed_transaction)
{
    Coin balance = impl.m_state.get_balance(signed_transaction.authority);
    if (coin(balance) < signed_transaction.transaction_details.fee)
        return false;
    return true;
}

void fee_apply(publiqpp::detail::node_internals& impl,
               SignedTransaction const& signed_transaction,
               string const& fee_receiver)
{
    if (false == fee_receiver.empty())
    {
        auto const& fee = signed_transaction.transaction_details.fee;
        Coin balance = impl.m_state.get_balance(signed_transaction.authority);
        if (coin(balance) < fee)
            throw not_enough_balance_exception(coin(balance),
                                               fee);

        impl.m_state.increase_balance(fee_receiver, fee);
        impl.m_state.decrease_balance(signed_transaction.authority, fee);
    }
}

void fee_revert(publiqpp::detail::node_internals& impl,
                SignedTransaction const& signed_transaction,
                string const& fee_receiver)
{
    if (false == fee_receiver.empty())
    {
        auto const& fee = signed_transaction.transaction_details.fee;
        Coin balance = impl.m_state.get_balance(fee_receiver);
        if (coin(balance) < fee)
            throw not_enough_balance_exception(coin(balance),
                                               fee);

        impl.m_state.decrease_balance(fee_receiver, fee);
        impl.m_state.increase_balance(signed_transaction.authority, fee);
    }
}

template <typename T_action>
bool action_process_on_chain_t(BlockchainMessage::SignedTransaction const& signed_transaction,
                               T_action const& action,
                               publiqpp::detail::node_internals& impl)
{
    action_validate(signed_transaction, action);

    // Don't need to store transaction if sync in process
    // and seems is too far from current block.
    // Just will check the transaction and broadcast

    //  need to define this condition otherwise
    /*if (impl.sync_headers.size() > BLOCK_TR_LENGTH)
        return true;*/

    // Check pool
    std::string tr_hash = meshpp::hash(signed_transaction.to_string());

    if (impl.m_transaction_cache.count(tr_hash))
        return false;

    auto transaction_cache_backup = impl.m_transaction_cache;

    beltpp::on_failure guard([&impl, &transaction_cache_backup]
    {
        impl.discard();
        impl.m_transaction_cache = std::move(transaction_cache_backup);
    });

    // Validate and add to state
    action_apply(impl, action);

    //  only validate the fee, but don't apply it
    fee_validate(impl, signed_transaction);

    // Add to the pool
    impl.m_transaction_pool.push_back(signed_transaction);
    impl.m_transaction_cache[tr_hash] =
        system_clock::from_time_t(signed_transaction.transaction_details.creation.tm);

    // Add to action log
    impl.m_action_log.log_transaction(signed_transaction);

    impl.save(guard);

    return true;
}
}
