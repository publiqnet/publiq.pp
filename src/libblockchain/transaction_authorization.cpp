#include "transaction_authorization.hpp"
#include "common.hpp"
#include "node_internals.hpp"
#include "exception.hpp"

#include <mesh.pp/cryptoutility.hpp>

using namespace BlockchainMessage;
using std::string;
using std::vector;

namespace publiqpp
{

//
// Award Authorization
//

vector<string> action_owners(AwardAuthorization const& award_authorization)
{
    return {award_authorization.owner, award_authorization.actor};
}
vector<string> action_participants(AwardAuthorization const& award_authorization)
{
    return {award_authorization.owner, award_authorization.actor};
}

void action_validate(SignedTransaction const& signed_transaction,
                     AwardAuthorization const& award_authorization,
                     bool check_complete)
{
    //  this is checked in signed_transaction_validate
    assert(false == signed_transaction.authorizations.empty());

    if (check_complete)
    {
        if (signed_transaction.authorizations.size() > action_owners(award_authorization).size())
            throw authority_exception(signed_transaction.authorizations.back().address, string());
        else if (signed_transaction.authorizations.size() < action_owners(award_authorization).size())
            throw authority_exception(string(), action_owners(award_authorization).back());
    }
    else
    {
        if (signed_transaction.authorizations.size() != 1)
            throw authority_exception(signed_transaction.authorizations[1].address, string());
    }

    if (award_authorization.owner == award_authorization.actor)
        throw wrong_data_exception("cannot award authorization to self");

    meshpp::public_key pb_key_from(award_authorization.owner);
    meshpp::public_key pb_key_actor(award_authorization.actor);
}

bool action_is_complete(SignedTransaction const& signed_transaction,
                        AwardAuthorization const& award_authorization)
{
    return (signed_transaction.authorizations.size() == action_owners(award_authorization).size());
}

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      SignedTransaction const& signed_transaction,
                      AwardAuthorization const& award_authorization,
                      state_layer/* layer*/)
{
    if (action_is_complete(signed_transaction, award_authorization))
    {
        {
            auto const& signer = signed_transaction.authorizations.front().address;
            auto const& owner = award_authorization.owner;

            if (false == impl.m_authority_manager.check_authority(owner, signer, AwardAuthorization::rtt))
                return false;
        }
        {
            auto const& signer = signed_transaction.authorizations.back().address;
            auto const& owner = award_authorization.actor;

            if (false == impl.m_authority_manager.check_authority(owner, signer, AwardAuthorization::rtt))
                return false;
        }
    }

    auto auth_record = impl.m_authority_manager.get_record(award_authorization.owner, award_authorization.actor);

    if (award_authorization.action_ids.empty()) // try to add full authorization
    {
        if (auth_record.default_full || false == auth_record.action_ids.empty())
            return false;

        //auth_record.default_full = true;
    }
    else
    {
        if (auth_record.default_full)
        {
            for (auto action_id : award_authorization.action_ids)
            {
                size_t erased = auth_record.action_ids.erase(action_id);

                if (0 == erased)
                    return false;
            }
        }
        else
        {
            for (auto action_id : award_authorization.action_ids)
            {
                auto insert_res = auth_record.action_ids.insert(action_id);

                if (false == insert_res.second)
                    return false;
            }
        }
    }

    return true;
}

void action_apply(publiqpp::detail::node_internals& impl,
                  SignedTransaction const& signed_transaction,
                  AwardAuthorization const& award_authorization,
                  state_layer layer)
{
    if (action_is_complete(signed_transaction, award_authorization))
    {
        {
            auto const& signer = signed_transaction.authorizations.front().address;
            auto const& owner = award_authorization.owner;

            if (false == impl.m_authority_manager.check_authority(owner, signer, AwardAuthorization::rtt))
                throw authority_exception(signer, impl.m_authority_manager.get_authority(owner, AwardAuthorization::rtt));
        }
        {
            auto const& signer = signed_transaction.authorizations.back().address;
            auto const& owner = award_authorization.actor;

            if (false == impl.m_authority_manager.check_authority(owner, signer, AwardAuthorization::rtt))
                throw authority_exception(signer, impl.m_authority_manager.get_authority(owner, AwardAuthorization::rtt));
        }
    }

    impl.m_authority_manager.smart_create_dummy_record(award_authorization.owner, award_authorization.actor);

    auto auth_record = impl.m_authority_manager.get_record(award_authorization.owner, award_authorization.actor);

    if (award_authorization.action_ids.empty()) // try to add full authorization
    {
        if (auth_record.default_full || false == auth_record.action_ids.empty())
            throw wrong_data_exception(award_authorization.actor + " is not rejected full authorizations before awarding full");

        auth_record.default_full = true;
    }
    else
    {
        if (auth_record.default_full)
        {
            for (auto action_id : award_authorization.action_ids)
            {
                size_t erased = auth_record.action_ids.erase(action_id);

                if (0 == erased)
                    throw wrong_data_exception(award_authorization.actor + " is not rejected authorization for action id: " + std::to_string(action_id));
            }
        }
        else
        {
            for (auto action_id : award_authorization.action_ids)
            {
                auto insert_res = auth_record.action_ids.insert(action_id);

                if (false == insert_res.second)
                    throw wrong_data_exception(award_authorization.actor + " is awarded authorization for action id: " + std::to_string(action_id));
            }
        }
    }

    impl.m_authority_manager.set_record(award_authorization.owner, award_authorization.actor, auth_record);
    
    impl.m_authority_manager.smart_cleanup_dummy_record(award_authorization.owner, award_authorization.actor);
}

void action_revert(publiqpp::detail::node_internals& impl,
                   SignedTransaction const& signed_transaction,
                   AwardAuthorization const& award_authorization,
                   state_layer layer)
{
    impl.m_authority_manager.smart_create_dummy_record(award_authorization.owner, award_authorization.actor);

    auto auth_record = impl.m_authority_manager.get_record(award_authorization.owner, award_authorization.actor);

    if (award_authorization.action_ids.empty()) // try to reject full authorization
    {
        if (false == auth_record.default_full || false == auth_record.action_ids.empty())
            throw std::logic_error("action_revert: " + award_authorization.actor + " is not awarded full authorizations before rejecting full");

        auth_record.default_full = false;
    }
    else
    {
        if (auth_record.default_full)
        {
            for (auto action_id : award_authorization.action_ids)
            {
                auto insert_res = auth_record.action_ids.insert(action_id);

                if (false == insert_res.second)
                    throw std::logic_error("action_revert: " + award_authorization.actor + " is rejected authorization for action id: " + std::to_string(action_id));
            }
        }
        else
        {
            for (auto action_id : award_authorization.action_ids)
            {
                size_t erased = auth_record.action_ids.erase(action_id);

                if (0 == erased)
                    throw std::logic_error("action_revert: " + award_authorization.actor + " is not awarded authorization for action id: " + std::to_string(action_id));
            }
        }
    }

    impl.m_authority_manager.set_record(award_authorization.owner, award_authorization.actor, auth_record);
    
    impl.m_authority_manager.smart_cleanup_dummy_record(award_authorization.owner, award_authorization.actor);
}

//
// Reject Authorization
//

vector<string> action_owners(RejectAuthorization const& reject_authorization)
{
    return {reject_authorization.owner};
}
vector<string> action_participants(RejectAuthorization const& reject_authorization)
{
    return {reject_authorization.owner, reject_authorization.actor};
}

void action_validate(SignedTransaction const& signed_transaction,
                     RejectAuthorization const& reject_authorization,
                     bool/* check_complete*/)
{
    //  this is checked in signed_transaction_validate
    assert(false == signed_transaction.authorizations.empty());

    if (signed_transaction.authorizations.size() != 1)
        throw authority_exception(signed_transaction.authorizations.back().address, string());

    meshpp::public_key pb_key_from(reject_authorization.owner);
    meshpp::public_key pb_key_actor(reject_authorization.actor);
}

bool action_is_complete(SignedTransaction const&/* signed_transaction*/,
                        RejectAuthorization const& reject_authorization)
{
    return true;
}

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      SignedTransaction const& signed_transaction,
                      RejectAuthorization const& reject_authorization,
                      state_layer/* layer*/)
{
    assert(signed_transaction.authorizations.size() == 1);

    auto const& signer = signed_transaction.authorizations.front().address;
    auto const& owner = reject_authorization.owner;

    if (false == impl.m_authority_manager.check_authority(owner, signer, RejectAuthorization::rtt))
        return false;

    auto auth_record = impl.m_authority_manager.get_record(reject_authorization.owner, reject_authorization.actor);

    if (reject_authorization.action_ids.empty()) // try to reject full authorization
    {
        if (false == auth_record.default_full || false == auth_record.action_ids.empty())
            return false;

        //auth_record.default_full = false;
    }
    else
    {
        if (auth_record.default_full)
        {
            for (auto action_id : reject_authorization.action_ids)
            {
                auto insert_res = auth_record.action_ids.insert(action_id);

                if (false == insert_res.second)
                    return false;
            }
        }
        else
        {
            for (auto action_id : reject_authorization.action_ids)
            {
                size_t erased = auth_record.action_ids.erase(action_id);

                if (0 == erased)
                    return false;
            }
        }
    }

    return true;
}

void action_apply(publiqpp::detail::node_internals& impl,
                  SignedTransaction const& signed_transaction,
                  RejectAuthorization const& reject_authorization,
                  state_layer layer)
{
    assert(signed_transaction.authorizations.size() == 1);

    auto const& signer = signed_transaction.authorizations.front().address;
    auto const& owner = reject_authorization.owner;

    if (false == impl.m_authority_manager.check_authority(owner, signer, RejectAuthorization::rtt))
        throw authority_exception(signer, impl.m_authority_manager.get_authority(owner, RejectAuthorization::rtt));

    impl.m_authority_manager.smart_create_dummy_record(reject_authorization.owner, reject_authorization.actor);

    auto auth_record = impl.m_authority_manager.get_record(reject_authorization.owner, reject_authorization.actor);

    if (reject_authorization.action_ids.empty()) // try to reject full authorization
    {
        if (false == auth_record.default_full || false == auth_record.action_ids.empty())
            throw wrong_data_exception(reject_authorization.actor + " is not awarded full authorizations before rejecting full");

        auth_record.default_full = false;
    }
    else
    {
        if (auth_record.default_full)
        {
            for (auto action_id : reject_authorization.action_ids)
            {
                auto insert_res = auth_record.action_ids.insert(action_id);

                if (false == insert_res.second)
                    throw wrong_data_exception(reject_authorization.actor + " is rejected authorization for action id: " + std::to_string(action_id));
            }
        }
        else
        {
            for (auto action_id : reject_authorization.action_ids)
            {
                size_t erased = auth_record.action_ids.erase(action_id);

                if (0 == erased)
                    throw wrong_data_exception(reject_authorization.actor + " is not awarded authorization for action id: " + std::to_string(action_id));
            }
        }
    }

    impl.m_authority_manager.set_record(reject_authorization.owner, reject_authorization.actor, auth_record);
    
    impl.m_authority_manager.smart_cleanup_dummy_record(reject_authorization.owner, reject_authorization.actor);
}

void action_revert(publiqpp::detail::node_internals& impl,
                   SignedTransaction const& signed_transaction,
                   RejectAuthorization const& reject_authorization,
                   state_layer layer)
{
    impl.m_authority_manager.smart_create_dummy_record(reject_authorization.owner, reject_authorization.actor);

    auto auth_record = impl.m_authority_manager.get_record(reject_authorization.owner, reject_authorization.actor);

    if (reject_authorization.action_ids.empty()) // try to add full authorization
    {
        if (auth_record.default_full || false == auth_record.action_ids.empty())
            throw std::logic_error("action_revert: " + reject_authorization.actor + " is not rejected full authorizations before awarding full");

        auth_record.default_full = true;
    }
    else
    {
        if (auth_record.default_full)
        {
            for (auto action_id : reject_authorization.action_ids)
            {
                size_t erased = auth_record.action_ids.erase(action_id);

                if (0 == erased)
                    throw std::logic_error("action_revert: " + reject_authorization.actor + " is not rejected authorization for action id: " + std::to_string(action_id));
            }
        }
        else
        {
            for (auto action_id : reject_authorization.action_ids)
            {
                auto insert_res = auth_record.action_ids.insert(action_id);

                if (false == insert_res.second)
                    throw std::logic_error("action_revert: " + reject_authorization.actor + " is awarded authorization for action id: " + std::to_string(action_id));
            }
        }
    }

    impl.m_authority_manager.set_record(reject_authorization.owner, reject_authorization.actor, auth_record);
    
    impl.m_authority_manager.smart_cleanup_dummy_record(reject_authorization.owner, reject_authorization.actor);
}
}
