#include "transaction_sponsoring.hpp"
#include "common.hpp"
#include "node_internals.hpp"
#include "exception.hpp"

#include <mesh.pp/cryptoutility.hpp>

using namespace BlockchainMessage;
using std::string;
using std::vector;

namespace publiqpp
{
vector<string> action_owners(SponsorContentUnit const& sponsor_content_unit)
{
    return {sponsor_content_unit.sponsor_address};
}
vector<string> action_participants(SponsorContentUnit const& sponsor_content_unit)
{
    return {sponsor_content_unit.uri, sponsor_content_unit.sponsor_address};
}

void action_validate(SignedTransaction const& signed_transaction,
                     SponsorContentUnit const& sponsor_content_unit,
                     bool/* check_complete*/)
{
    //  this is checked in signed_transaction_validate
    assert(false == signed_transaction.authorizations.empty());

    if (signed_transaction.authorizations.size() != 1)
        throw authority_exception(signed_transaction.authorizations.back().address, string());

    auto signed_authority = signed_transaction.authorizations.front().address;
    if (signed_authority != sponsor_content_unit.sponsor_address)
        throw authority_exception(signed_authority, sponsor_content_unit.sponsor_address);

    if (sponsor_content_unit.hours == 0)
        throw wrong_data_exception("sponsor duration");
    if (sponsor_content_unit.amount == Coin())
        throw wrong_data_exception("sponsor amount");

    meshpp::public_key pb_key_from(sponsor_content_unit.sponsor_address);

    string file_hash = meshpp::from_base58(sponsor_content_unit.uri);
    if (file_hash.length() != 32)
        throw uri_exception(sponsor_content_unit.uri, uri_exception::invalid);
}

bool action_is_complete(SignedTransaction const&/* signed_transaction*/,
                        SponsorContentUnit const&/* sponsor_content_unit*/)
{
    return true;
}

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      SignedTransaction const&/* signed_transaction*/,
                      SponsorContentUnit const& sponsor_content_unit,
                      state_layer/* layer*/)
{
    if (false == impl.m_documents.exist_unit(sponsor_content_unit.uri))
        return false;

    Coin balance = impl.m_state.get_balance(sponsor_content_unit.sponsor_address, state_layer::pool);
    if (coin(balance) < sponsor_content_unit.amount)
        return false;

    return true;
}

void action_apply(publiqpp::detail::node_internals& impl,
                  SignedTransaction const&/* signed_transaction*/,
                  SponsorContentUnit const& sponsor_content_unit,
                  state_layer layer)
{
    if (false == impl.m_documents.exist_unit(sponsor_content_unit.uri))
        throw uri_exception(sponsor_content_unit.uri, uri_exception::missing);

    /*  this is written already in decrease_balance
    Coin balance = impl.m_state.get_balance(sponsor_content_unit.sponsor_address, state_layer::pool);
    if (coin(balance) < sponsor_content_unit.amount)
        throw not_enough_balance_exception(coin(balance),
                                           transfer.amount);
    */

    impl.m_state.decrease_balance(sponsor_content_unit.sponsor_address,
                                  sponsor_content_unit.amount,
                                  layer);

    impl.m_documents.sponsor_content_unit_apply(sponsor_content_unit);
}

void action_revert(publiqpp::detail::node_internals& impl,
                   SignedTransaction const&/* signed_transaction*/,
                   SponsorContentUnit const& sponsor_content_unit,
                   state_layer layer)
{
    impl.m_state.increase_balance(sponsor_content_unit.sponsor_address,
                                  sponsor_content_unit.amount,
                                  layer);

    impl.m_documents.sponsor_content_unit_revert(sponsor_content_unit);
}
}
