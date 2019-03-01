#include "global.hpp"
#include "transaction_handler.hpp"

using namespace BlockchainMessage;

namespace publiqpp
{
bool action_process(SignedTransaction const& signed_transaction,
                    std::unique_ptr<publiqpp::detail::node_internals>& pimpl)
{
    auto const& package = signed_transaction.transaction_details.action;
    switch (package.type())
    {
    case Transfer::rtt:
    {
        Transfer const* paction;
        package.get(paction);
        return action_process(signed_transaction, *paction, pimpl);
    }
    default:
        throw wrong_data_exception("unknown transaction action type!");
    }
}

void action_validate(BlockchainMessage::SignedTransaction const& signed_transaction)
{
    auto const& package = signed_transaction.transaction_details.action;
    switch (package.type())
    {
    case Transfer::rtt:
    {
        Transfer const* paction;
        package.get(paction);
        return action_validate(signed_transaction, *paction);
    }
    default:
        throw wrong_data_exception("unknown transaction action type!");
    }
}

bool action_can_apply(std::unique_ptr<publiqpp::detail::node_internals> const& pimpl,
                      beltpp::packet const& package)
{
    switch (package.type())
    {
    case Transfer::rtt:
    {
        Transfer const* paction;
        package.get(paction);
        return action_can_apply(pimpl, *paction);
    }
    default:
        throw wrong_data_exception("unknown transaction action type!");
    }
}

void action_apply(std::unique_ptr<publiqpp::detail::node_internals>& pimpl,
                  beltpp::packet const& package)
{
    switch (package.type())
    {
    case Transfer::rtt:
    {
        Transfer const* paction;
        package.get(paction);
        return action_apply(pimpl, *paction);
    }
    default:
        throw wrong_data_exception("unknown transaction action type!");
    }
}

void action_revert(std::unique_ptr<publiqpp::detail::node_internals>& pimpl,
                   beltpp::packet const& package)
{
    switch (package.type())
    {
    case Transfer::rtt:
    {
        Transfer const* paction;
        package.get(paction);
        return action_revert(pimpl, *paction);
    }
    default:
        throw wrong_data_exception("unknown transaction action type!");
    }
}

void fee_validate(std::unique_ptr<publiqpp::detail::node_internals> const& pimpl,
                  BlockchainMessage::SignedTransaction const& signed_transaction)
{
    Coin balance = pimpl->m_state.get_balance(signed_transaction.authority);
    if (coin(balance) < signed_transaction.transaction_details.fee)
        throw not_enough_balance_exception(coin(balance),
                                           signed_transaction.transaction_details.fee);
}

bool fee_can_apply(std::unique_ptr<publiqpp::detail::node_internals> const& pimpl,
                   SignedTransaction const& signed_transaction)
{
    Coin balance = pimpl->m_state.get_balance(signed_transaction.authority);
    if (coin(balance) < signed_transaction.transaction_details.fee)
        return false;
    return true;
}

void fee_apply(std::unique_ptr<publiqpp::detail::node_internals>& pimpl,
               SignedTransaction const& signed_transaction,
               string const& fee_receiver)
{
    if (false == fee_receiver.empty())
    {
        auto const& fee = signed_transaction.transaction_details.fee;
        Coin balance = pimpl->m_state.get_balance(signed_transaction.authority);
        if (coin(balance) < fee)
            throw not_enough_balance_exception(coin(balance),
                                               fee);

        pimpl->m_state.increase_balance(fee_receiver, fee);
        pimpl->m_state.decrease_balance(signed_transaction.authority, fee);
    }
}

void fee_revert(std::unique_ptr<publiqpp::detail::node_internals>& pimpl,
                SignedTransaction const& signed_transaction,
                string const& fee_receiver)
{
    if (false == fee_receiver.empty())
    {
        auto const& fee = signed_transaction.transaction_details.fee;
        Coin balance = pimpl->m_state.get_balance(fee_receiver);
        if (coin(balance) < fee)
            throw not_enough_balance_exception(coin(balance),
                                               fee);

        pimpl->m_state.decrease_balance(fee_receiver, fee);
        pimpl->m_state.increase_balance(signed_transaction.authority, fee);
    }
}
}
