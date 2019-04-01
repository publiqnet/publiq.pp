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
void signed_transaction_validate(SignedTransaction& signed_transaction,
                                 std::chrono::system_clock::time_point const& now,
                                 std::chrono::seconds const& time_shift)
{
    if (signed_transaction.authorizations.empty())
        throw wrong_data_exception("transaction with no authorizations");

    string prev_signature;
    for (auto const& authority : signed_transaction.authorizations)
    {
        meshpp::public_key pb_key(authority.address);
        string signed_message;
        if (prev_signature.empty())
            signed_message = signed_transaction.transaction_details.to_string();
        else
            signed_message = prev_signature;

        meshpp::signature signature_check(pb_key, signed_message, authority.signature);

        prev_signature = authority.signature;
    }

    namespace chrono = std::chrono;
    using chrono::system_clock;
    using time_point = system_clock::time_point;
    time_point creation = system_clock::from_time_t(signed_transaction.transaction_details.creation.tm);
    time_point expiry = system_clock::from_time_t(signed_transaction.transaction_details.expiry.tm);

    if (now + time_shift < creation)
        throw wrong_data_exception("Transaction from the future!");

    if (now - time_shift > expiry)
        throw wrong_data_exception("Expired transaction!");

    if (expiry - creation > std::chrono::hours(TRANSACTION_MAX_LIFETIME_HOURS))
        throw wrong_data_exception("Too long lifetime for transaction");
}

template <typename T_action>
broadcast_type action_process_on_chain_t(SignedTransaction& signed_transaction,
                                         T_action const& action,
                                         publiqpp::detail::node_internals& impl);

broadcast_type action_process_on_chain(SignedTransaction& signed_transaction,
                                       publiqpp::detail::node_internals& impl)
{
    broadcast_type code;
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
    case StorageUpdate::rtt:
    {
        StorageUpdate const* paction;
        package.get(paction);
        code = action_process_on_chain_t(signed_transaction, *paction, impl);
        break;
    }
    case ServiceStatistics::rtt:
    {
        ServiceStatistics const* paction;
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
                     SignedTransaction const& signed_transaction,
                     bool check_complete)
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
        action_validate(signed_transaction, *paction, check_complete);
        break;
    }
    case File::rtt:
    {
        File const* paction;
        package.get(paction);
        action_validate(signed_transaction, *paction, check_complete);
        break;
    }
    case ContentUnit::rtt:
    {
        ContentUnit const* paction;
        package.get(paction);
        action_validate(signed_transaction, *paction, check_complete);
        break;
    }
    case Content::rtt:
    {
        Content const* paction;
        package.get(paction);
        action_validate(signed_transaction, *paction, check_complete);
        break;
    }
    case Role::rtt:
    {
        Role const* paction;
        package.get(paction);
        action_validate(signed_transaction, *paction, check_complete);
        break;
    }
    case StorageUpdate::rtt:
    {
        StorageUpdate const* paction;
        package.get(paction);
        action_validate(signed_transaction, *paction, check_complete);
        break;
    }
    case ServiceStatistics::rtt:
    {
        ServiceStatistics const* paction;
        package.get(paction);
        action_validate(signed_transaction, *paction, check_complete);
        break;
    }
    default:
        throw wrong_data_exception("unknown transaction action type!");
    }
}

authorization_process_result action_authorization_process(publiqpp::detail::node_internals& impl,
                                                          SignedTransaction& signed_transaction)
{
    authorization_process_result code;
    auto const& package = signed_transaction.transaction_details.action;

    if (impl.m_transfer_only && package.type() != Transfer::rtt)
        throw std::runtime_error("this is coin only blockchain");

    switch (package.type())
    {
    case Transfer::rtt:
    {
        Transfer const* paction;
        package.get(paction);
        code = action_authorization_process(signed_transaction, *paction);
        break;
    }
    case File::rtt:
    {
        File const* paction;
        package.get(paction);
        code = action_authorization_process(signed_transaction, *paction);
        break;
    }
    case ContentUnit::rtt:
    {
        ContentUnit const* paction;
        package.get(paction);
        code = action_authorization_process(signed_transaction, *paction);
        break;
    }
    case Content::rtt:
    {
        Content const* paction;
        package.get(paction);
        code = action_authorization_process(signed_transaction, *paction);
        break;
    }
    case Role::rtt:
    {
        Role const* paction;
        package.get(paction);
        code = action_authorization_process(signed_transaction, *paction);
        break;
    }
    case StorageUpdate::rtt:
    {
        StorageUpdate const* paction;
        package.get(paction);
        code = action_authorization_process(signed_transaction, *paction);
        break;
    }
    case ServiceStatistics::rtt:
    {
        ServiceStatistics const* paction;
        package.get(paction);
        code = action_authorization_process(signed_transaction, *paction);
        break;
    }
    default:
        throw wrong_data_exception("unknown transaction action type!");
    }

    return code;
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
    case StorageUpdate::rtt:
    {
        StorageUpdate const* paction;
        package.get(paction);
        code = action_can_apply(impl, *paction);
        break;
    }
    case ServiceStatistics::rtt:
    {
        ServiceStatistics const* paction;
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
                  beltpp::packet const& package,
                  state_layer layer)
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
        action_apply(impl, *paction, layer);
        break;
    }
    case File::rtt:
    {
        File const* paction;
        package.get(paction);
        action_apply(impl, *paction, layer);
        break;
    }
    case ContentUnit::rtt:
    {
        ContentUnit const* paction;
        package.get(paction);
        action_apply(impl, *paction, layer);
        break;
    }
    case Content::rtt:
    {
        Content const* paction;
        package.get(paction);
        action_apply(impl, *paction, layer);
        break;
    }
    case Role::rtt:
    {
        Role const* paction;
        package.get(paction);
        action_apply(impl, *paction, layer);
        break;
    }
    case StorageUpdate::rtt:
    {
        StorageUpdate const* paction;
        package.get(paction);
        action_apply(impl, *paction, layer);
        break;
    }
    case ServiceStatistics::rtt:
    {
        ServiceStatistics const* paction;
        package.get(paction);
        action_apply(impl, *paction, layer);
        break;
    }
    default:
        throw wrong_data_exception("unknown transaction action type!");
    }
}

void action_revert(publiqpp::detail::node_internals& impl,
                   beltpp::packet const& package,
                   state_layer layer)
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
        action_revert(impl, *paction, layer);
        break;
    }
    case File::rtt:
    {
        File const* paction;
        package.get(paction);
        action_revert(impl, *paction, layer);
        break;
    }
    case ContentUnit::rtt:
    {
        ContentUnit const* paction;
        package.get(paction);
        action_revert(impl, *paction, layer);
        break;
    }
    case Content::rtt:
    {
        Content const* paction;
        package.get(paction);
        action_revert(impl, *paction, layer);
        break;
    }
    case Role::rtt:
    {
        Role const* paction;
        package.get(paction);
        action_revert(impl, *paction, layer);
        break;
    }
    case StorageUpdate::rtt:
    {
        StorageUpdate const* paction;
        package.get(paction);
        action_revert(impl, *paction, layer);
        break;
    }
    case ServiceStatistics::rtt:
    {
        ServiceStatistics const* paction;
        package.get(paction);
        action_revert(impl, *paction, layer);
        break;
    }
    default:
        throw wrong_data_exception("unknown transaction action type!");
    }
}

void fee_validate(publiqpp::detail::node_internals const& impl,
                  BlockchainMessage::SignedTransaction const& signed_transaction)
{
    Coin balance = impl.m_state.get_balance(signed_transaction.authorizations.front().address, state_layer::pool);
    if (coin(balance) < signed_transaction.transaction_details.fee)
        throw not_enough_balance_exception(coin(balance),
                                           signed_transaction.transaction_details.fee);
}

bool fee_can_apply(publiqpp::detail::node_internals const& impl,
                   SignedTransaction const& signed_transaction)
{
    Coin balance = impl.m_state.get_balance(signed_transaction.authorizations.front().address, state_layer::pool);
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
        Coin balance = impl.m_state.get_balance(signed_transaction.authorizations.front().address, state_layer::pool);
        if (coin(balance) < fee)
            throw not_enough_balance_exception(coin(balance),
                                               fee);

        impl.m_state.increase_balance(fee_receiver, fee, state_layer::chain);
        impl.m_state.decrease_balance(signed_transaction.authorizations.front().address, fee, state_layer::chain);
    }
}

void fee_revert(publiqpp::detail::node_internals& impl,
                SignedTransaction const& signed_transaction,
                string const& fee_receiver)
{
    if (false == fee_receiver.empty())
    {
        auto const& fee = signed_transaction.transaction_details.fee;
        Coin balance = impl.m_state.get_balance(fee_receiver, state_layer::pool);
        if (coin(balance) < fee)
            throw not_enough_balance_exception(coin(balance),
                                               fee);

        impl.m_state.decrease_balance(fee_receiver, fee, state_layer::chain);
        impl.m_state.increase_balance(signed_transaction.authorizations.front().address, fee, state_layer::chain);
    }
}

template <typename T_action>
broadcast_type action_process_on_chain_t(BlockchainMessage::SignedTransaction& signed_transaction,
                                         T_action const& action,
                                         publiqpp::detail::node_internals& impl)
{
    authorization_process_result code;
    code = action_authorization_process(impl, signed_transaction);

    broadcast_type on_success = broadcast_type::continue_broadcast;
    if (code.modified)
        on_success = broadcast_type::full_broadcast;

    action_validate(signed_transaction, action, code.complete);

    // Don't need to store transaction if sync in process
    // and seems is too far from current block.
    // Just will check the transaction and broadcast

    if (system_clock::from_time_t(impl.m_blockchain.last_header().time_signed.tm) < 
        system_clock::now() - chrono::seconds(BLOCK_TR_LENGTH * BLOCK_MINE_DELAY))
        return on_success;

    // Check pool
    if (impl.m_transaction_cache.contains(signed_transaction))
        return broadcast_type::none;

    impl.m_transaction_cache.backup();
    beltpp::on_failure guard([&impl]
    {
        impl.discard();
        impl.m_transaction_cache.restore();
    });

    if (code.complete ||
        false == action_can_apply(impl, action))
    {
        //  validate and add to state
        action_apply(impl, action, state_layer::pool);

        //  only validate the fee, but don't apply it
        fee_validate(impl, signed_transaction);

        // Add to action log
        impl.m_action_log.log_transaction(signed_transaction);
    }

    // Add to the pool
    impl.m_transaction_pool.push_back(signed_transaction);
    impl.m_transaction_cache.add_pool(signed_transaction, code.complete);

    impl.save(guard);

    return on_success;
}
}// end of namespace publiqpp
