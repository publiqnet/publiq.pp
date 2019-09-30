#include "transaction_sponsoring.hpp"
#include "common.hpp"
#include "node_internals.hpp"
#include "exception.hpp"

#include <mesh.pp/cryptoutility.hpp>

#include <chrono>
#include <map>

namespace chrono = std::chrono;
using chrono::system_clock;

using namespace BlockchainMessage;
using std::string;
using std::vector;
using std::map;

namespace publiqpp
{
// sponsoring stuff
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
    if (false == impl.m_documents.unit_exists(sponsor_content_unit.uri))
        return false;

    Coin balance = impl.m_state.get_balance(sponsor_content_unit.sponsor_address, state_layer::pool);
    if (coin(balance) < sponsor_content_unit.amount)
        return false;

    return true;
}

void action_apply(publiqpp::detail::node_internals& impl,
                  SignedTransaction const& signed_transaction,
                  SponsorContentUnit const& sponsor_content_unit,
                  state_layer layer)
{
    if (false == impl.m_documents.unit_exists(sponsor_content_unit.uri))
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

    impl.m_documents.sponsor_content_unit_apply(impl,
                                                sponsor_content_unit,
                                                meshpp::hash(signed_transaction.to_string()));
}

void action_revert(publiqpp::detail::node_internals& impl,
                   SignedTransaction const& signed_transaction,
                   SponsorContentUnit const& sponsor_content_unit,
                   state_layer layer)
{
    impl.m_state.increase_balance(sponsor_content_unit.sponsor_address,
                                  sponsor_content_unit.amount,
                                  layer);

    impl.m_documents.sponsor_content_unit_revert(impl,
                                                 sponsor_content_unit,
                                                 meshpp::hash(signed_transaction.to_string()));
}

// cancel sponsoring stuff

vector<string> action_owners(CancelSponsorContentUnit const& cancel_sponsor_content_unit)
{
    return {cancel_sponsor_content_unit.sponsor_address};
}
vector<string> action_participants(CancelSponsorContentUnit const& cancel_sponsor_content_unit)
{
    return {cancel_sponsor_content_unit.uri, cancel_sponsor_content_unit.sponsor_address};
}

void action_validate(SignedTransaction const& signed_transaction,
                     CancelSponsorContentUnit const& cancel_sponsor_content_unit,
                     bool/* check_complete*/)
{
    //  this is checked in signed_transaction_validate
    assert(false == signed_transaction.authorizations.empty());

    if (signed_transaction.authorizations.size() != 1)
        throw authority_exception(signed_transaction.authorizations.back().address, string());

    auto signed_authority = signed_transaction.authorizations.front().address;
    if (signed_authority != cancel_sponsor_content_unit.sponsor_address)
        throw authority_exception(signed_authority, cancel_sponsor_content_unit.sponsor_address);

    meshpp::public_key pb_key_from(cancel_sponsor_content_unit.sponsor_address);

    string file_hash = meshpp::from_base58(cancel_sponsor_content_unit.uri);
    if (file_hash.length() != 32)
        throw uri_exception(cancel_sponsor_content_unit.uri, uri_exception::invalid);

    string transaction_hash = meshpp::from_base58(cancel_sponsor_content_unit.transaction_hash);
    if (transaction_hash.length() != 32)
        throw wrong_data_exception("invalid transaction hash: " +
                                   cancel_sponsor_content_unit.transaction_hash);
}

bool action_is_complete(SignedTransaction const&/* signed_transaction*/,
                        CancelSponsorContentUnit const&/* cancel_sponsor_content_unit*/)
{
    return true;
}

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      SignedTransaction const&/* signed_transaction*/,
                      CancelSponsorContentUnit const& cancel_sponsor_content_unit,
                      state_layer/* layer*/)
{
    if (false == impl.m_documents.unit_exists(cancel_sponsor_content_unit.uri))
        return false;

    map<string, coin> temp_sponsored_rewards =
        const_cast<publiqpp::detail::node_internals&>(impl).
            m_documents.sponsored_content_unit_set_used(
                    impl,
                    cancel_sponsor_content_unit.uri,
                    impl.m_blockchain.length(),
                    documents::sponsored_content_unit_set_used_apply,
                    cancel_sponsor_content_unit.transaction_hash,
                    cancel_sponsor_content_unit.sponsor_address,
                    true);  //  pretend

    for (auto const& temp_sponsored_reward : temp_sponsored_rewards)
    {
        if (temp_sponsored_reward.second == coin())
            return false;
    }

    return true;
}

void action_apply(publiqpp::detail::node_internals& impl,
                  SignedTransaction const& /*signed_transaction*/,
                  CancelSponsorContentUnit const& cancel_sponsor_content_unit,
                  state_layer /*layer*/)
{
    if (false == impl.m_documents.unit_exists(cancel_sponsor_content_unit.uri))
        throw uri_exception(cancel_sponsor_content_unit.uri, uri_exception::missing);

    map<string, coin> temp_sponsored_rewards =
        impl.m_documents.sponsored_content_unit_set_used(
                    impl,
                    cancel_sponsor_content_unit.uri,
                    impl.m_blockchain.length(),
                    documents::sponsored_content_unit_set_used_apply,
                    cancel_sponsor_content_unit.transaction_hash,
                    cancel_sponsor_content_unit.sponsor_address,
                    false);  //  pretend

    for (auto const& temp_sponsored_reward : temp_sponsored_rewards)
    {
        if (temp_sponsored_reward.second == coin())
            throw wrong_data_exception("invalid transaction hash: " +
                                       cancel_sponsor_content_unit.transaction_hash);
    }
}

void action_revert(publiqpp::detail::node_internals& impl,
                   SignedTransaction const& /*signed_transaction*/,
                   CancelSponsorContentUnit const& cancel_sponsor_content_unit,
                   state_layer /*layer*/)
{
    map<string, coin> temp_sponsored_rewards =
        impl.m_documents.sponsored_content_unit_set_used(
                    impl,
                    cancel_sponsor_content_unit.uri,
                    impl.m_blockchain.length(),
                    documents::sponsored_content_unit_set_used_revert,
                    cancel_sponsor_content_unit.transaction_hash,
                    cancel_sponsor_content_unit.sponsor_address,
                    false);  // pretend

    for (auto const& temp_sponsored_reward : temp_sponsored_rewards)
    {
        assert(temp_sponsored_reward.second != coin());
        if (temp_sponsored_reward.second == coin())
            throw std::logic_error("invalid transaction hash: " +
                                   cancel_sponsor_content_unit.transaction_hash);
    }
}
}
