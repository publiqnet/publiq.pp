#include "transaction_transfer.hpp"
#include "common.hpp"
#include "node_internals.hpp"
#include "exception.hpp"

#include <mesh.pp/cryptoutility.hpp>

using namespace BlockchainMessage;
using std::string;
using std::vector;

namespace publiqpp
{
vector<string> action_owners(Transfer const& transfer)
{
    return {transfer.from};
}
vector<string> action_participants(Transfer const& transfer)
{
    return {transfer.from, transfer.to};
}

void action_validate(SignedTransaction const& signed_transaction,
                     Transfer const& transfer,
                     bool/* check_complete*/)
{
    //  this is checked in signed_transaction_validate
    assert(false == signed_transaction.authorizations.empty());

    if (transfer.from == transfer.to ||
        transfer.amount == Coin())
        throw wrong_data_exception("dummy transfer");

    if (signed_transaction.authorizations.size() != 1)
        throw authority_exception(signed_transaction.authorizations.back().address, string());

    meshpp::public_key pb_key_to(transfer.to);
    meshpp::public_key pb_key_from(transfer.from);

    if (transfer.message.size() > 80)
        throw too_long_string_exception(transfer.message, 80);
}

bool action_is_complete(SignedTransaction const&/* signed_transaction*/,
                        Transfer const&/* transfer*/)
{
    return true;
}

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      SignedTransaction const& signed_transaction,
                      Transfer const& transfer,
                      state_layer/* layer*/)
{
    assert(signed_transaction.authorizations.size() == 1);

    auto signed_authority = signed_transaction.authorizations.front().address;
    if (false == impl.m_authority_manager.check_authority(transfer.from, signed_authority, Transfer::rtt))
        return false;

    Coin balance = impl.m_state.get_balance(transfer.from, state_layer::pool);
    if (coin(balance) < transfer.amount)
        return false;
    return true;
}

void action_apply(publiqpp::detail::node_internals& impl,
                  SignedTransaction const& signed_transaction,
                  Transfer const& transfer,
                  state_layer layer)
{
    assert(signed_transaction.authorizations.size() == 1);

    auto signed_authority = signed_transaction.authorizations.front().address;
    if (false == impl.m_authority_manager.check_authority(transfer.from, signed_authority, Transfer::rtt))
        throw authority_exception(signed_authority, impl.m_authority_manager.get_authority(transfer.from, Transfer::rtt));

    /*  this is written already in decrease_balance
    Coin balance = impl.m_state.get_balance(transfer.from, state_layer::pool);
    if (coin(balance) < transfer.amount)
        throw not_enough_balance_exception(coin(balance),
                                           transfer.amount);
    */

    impl.m_state.increase_balance(transfer.to, transfer.amount, layer);
    impl.m_state.decrease_balance(transfer.from, transfer.amount, layer);
}

void action_revert(publiqpp::detail::node_internals& impl,
                   SignedTransaction const& signed_transaction,
                   Transfer const& transfer,
                   state_layer layer)
{
    /*  this is written already in decrease_balance
    Coin balance = impl.m_state.get_balance(transfer.to, state_layer::pool);
    if (coin(balance) < transfer.amount)
        throw not_enough_balance_exception(coin(balance),
                                           transfer.amount);
    */

    impl.m_state.increase_balance(transfer.from, transfer.amount, layer);
    impl.m_state.decrease_balance(transfer.to, transfer.amount, layer);

    assert(signed_transaction.authorizations.size() == 1);

    auto signed_authority = signed_transaction.authorizations.front().address;
    if (false == impl.m_authority_manager.check_authority(transfer.from, signed_authority, Transfer::rtt))
        throw std::logic_error("false == impl.m_authority_manager.check_authority(transfer.from, signed_authority, Transfer::rtt)");
}
}
