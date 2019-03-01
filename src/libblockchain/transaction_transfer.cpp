#include "transaction_transfer.hpp"
#include "common.hpp"
#include "node_internals.hpp"
#include "exception.hpp"

#include <mesh.pp/cryptoutility.hpp>

using namespace BlockchainMessage;

namespace publiqpp
{
void action_validate(SignedTransaction const& signed_transaction,
                     Transfer const& transfer)
{
    if (signed_transaction.authority != transfer.from)
        throw authority_exception(signed_transaction.authority, transfer.from);

    meshpp::public_key pb_key_to(transfer.to);
    meshpp::public_key pb_key_from(transfer.from);

    if (transfer.message.size() > 80)
        throw too_long_string(transfer.message, 80);
}

bool action_can_apply(std::unique_ptr<publiqpp::detail::node_internals> const& pimpl,
                      Transfer const& transfer)
{
    Coin balance = pimpl->m_state.get_balance(transfer.from);
    if (coin(balance) < transfer.amount)
        return false;
    return true;
}

void action_apply(std::unique_ptr<publiqpp::detail::node_internals>& pimpl,
                  Transfer const& transfer)
{
    Coin balance = pimpl->m_state.get_balance(transfer.from);
    if (coin(balance) < transfer.amount)
        throw not_enough_balance_exception(coin(balance),
                                           transfer.amount);

    pimpl->m_state.increase_balance(transfer.to, transfer.amount);
    pimpl->m_state.decrease_balance(transfer.from, transfer.amount);
}

void action_revert(std::unique_ptr<publiqpp::detail::node_internals>& pimpl,
                   Transfer const& transfer)
{
    Coin balance = pimpl->m_state.get_balance(transfer.to);
    if (coin(balance) < transfer.amount)
        throw not_enough_balance_exception(coin(balance),
                                           transfer.amount);

    pimpl->m_state.increase_balance(transfer.from, transfer.amount);
    pimpl->m_state.decrease_balance(transfer.to, transfer.amount);
}
}
