#include "transaction_authorization.hpp"
#include "common.hpp"
#include "node_internals.hpp"
#include "exception.hpp"

#include <mesh.pp/cryptoutility.hpp>

using namespace BlockchainMessage;
using std::string;
using std::vector;
using std::unordered_set;

namespace publiqpp
{

vector<string> action_owners(AuthorizationUpdate const& authorization_update)
{
    return {authorization_update.owner};
}
vector<string> action_participants(AuthorizationUpdate const& authorization_update)
{
    return {authorization_update.owner, authorization_update.actor};
}

void action_validate(SignedTransaction const& signed_transaction,
                     AuthorizationUpdate const& authorization_update,
                     bool/* check_complete*/)
{
    //  this is checked in signed_transaction_validate
    assert(false == signed_transaction.authorizations.empty());

    if (signed_transaction.authorizations.size() != 1)
        throw authority_exception(signed_transaction.authorizations.back().address, string());

    if (signed_transaction.authorizations.front().address == authorization_update.actor)
        throw wrong_data_exception("cannot sign authorization update for self as an actor");

    unordered_set<uint64_t> allowed_ids =
    {
        AddressInfo::rtt,
        Letter::rtt,
        StorageUpdateCommand::rtt,
        Transfer::rtt,
        //AuthorizationUpdate::rtt,
        Role::rtt,
        File::rtt,
        ContentUnit::rtt,
        Content::rtt,
        SponsorContentUnit::rtt,
        CancelSponsorContentUnit::rtt,
        ServiceStatistics::rtt,
        StorageUpdate::rtt,
        Block::rtt,
    };

    for (uint64_t id : authorization_update.action_ids)
    {
        if (0 == allowed_ids.count(id))
            throw wrong_data_exception("unsupported customization for id: " + std::to_string(id));
    }

    meshpp::public_key pb_key_from(authorization_update.owner);
    meshpp::public_key pb_key_actor(authorization_update.actor);
}

bool action_is_complete(SignedTransaction const& signed_transaction,
                        AuthorizationUpdate const& authorization_update)
{
    return true;
}

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      SignedTransaction const& signed_transaction,
                      AuthorizationUpdate const& authorization_update,
                      state_layer/* layer*/)
{
    assert(signed_transaction.authorizations.size() == 1);

    auto const& signer = signed_transaction.authorizations.front().address;
    auto const& owner = authorization_update.owner;

    if (false == impl.m_authority_manager.check_authority(owner, signer, AuthorizationUpdate::rtt))
        return false;

    auto auth_record = impl.m_authority_manager.get_record(authorization_update.owner, authorization_update.actor);

    if (authorization_update.update_type == UpdateType::store &&
        authorization_update.action_ids.empty()) // try to award full authorization
    {
        if (auth_record.default_full || false == auth_record.action_ids.empty())
            return false;

        //auth_record.default_full = true;
    }
    else if (authorization_update.update_type == UpdateType::store)
    {
        if (auth_record.default_full)
        {
            for (auto action_id : authorization_update.action_ids)
            {
                size_t erased = auth_record.action_ids.erase(action_id);

                if (0 == erased)
                    return false;
                
                // limitation
                throw std::logic_error("action_can_apply: impossible to reach state that specifies 'default full' with exceptions");
            }
        }
        else
        {
            for (auto action_id : authorization_update.action_ids)
            {
                auto insert_res = auth_record.action_ids.insert(action_id);

                if (false == insert_res.second)
                    return false;
            }
        }
    }
    else if (authorization_update.action_ids.empty()) // try to reject full authorization
    {
        if (false == auth_record.default_full || false == auth_record.action_ids.empty())
            return false;

        //auth_record.default_full = false;
    }
    else
    {
        if (auth_record.default_full)
        {
            // limitation
            return false;

            // for (auto action_id : authorization_update.action_ids)
            // {
            //     auto insert_res = auth_record.action_ids.insert(action_id);

            //     if (false == insert_res.second)
            //         return false;
            // }
        }
        else
        {
            for (auto action_id : authorization_update.action_ids)
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
                  AuthorizationUpdate const& authorization_update,
                  state_layer layer)
{
    assert(signed_transaction.authorizations.size() == 1);

    auto const& signer = signed_transaction.authorizations.front().address;
    auto const& owner = authorization_update.owner;

    if (false == impl.m_authority_manager.check_authority(owner, signer, AuthorizationUpdate::rtt))
        throw authority_exception(signer, impl.m_authority_manager.get_authority(owner, AuthorizationUpdate::rtt));

    impl.m_authority_manager.smart_create_dummy_record(authorization_update.owner, authorization_update.actor);

    auto auth_record = impl.m_authority_manager.get_record(authorization_update.owner, authorization_update.actor);

    if (authorization_update.update_type == UpdateType::store &&
        authorization_update.action_ids.empty()) // try to award full authorization
    {
        if (auth_record.default_full || false == auth_record.action_ids.empty())
            throw wrong_data_exception(authorization_update.actor + " is not rejected full authorizations before awarding full");

        auth_record.default_full = true;
    }
    else if (authorization_update.update_type == UpdateType::store)
    {
        if (auth_record.default_full)
        {
            for (auto action_id : authorization_update.action_ids)
            {
                size_t erased = auth_record.action_ids.erase(action_id);

                if (0 == erased)
                    throw wrong_data_exception(authorization_update.actor + " is not rejected authorization for action id: " + std::to_string(action_id));

                // limitation
                throw std::logic_error("action_apply: impossible to reach state that specifies 'default full' with exceptions");
            }
        }
        else
        {
            for (auto action_id : authorization_update.action_ids)
            {
                auto insert_res = auth_record.action_ids.insert(action_id);

                if (false == insert_res.second)
                    throw wrong_data_exception(authorization_update.actor + " is awarded authorization for action id: " + std::to_string(action_id));
            }
        }
    }
    else if (authorization_update.action_ids.empty()) // try to reject full authorization
    {
        if (false == auth_record.default_full || false == auth_record.action_ids.empty())
            throw wrong_data_exception(authorization_update.actor + " is not awarded full authorizations before rejecting full");

        auth_record.default_full = false;
    }
    else
    {
        if (auth_record.default_full)
        {
            // limitation
            throw wrong_data_exception("parital authorization rejection is not supported");

            // for (auto action_id : authorization_update.action_ids)
            // {
            //     auto insert_res = auth_record.action_ids.insert(action_id);

            //     if (false == insert_res.second)
            //         throw wrong_data_exception(authorization_update.actor + " is rejected authorization for action id: " + std::to_string(action_id));
            // }
        }
        else
        {
            for (auto action_id : authorization_update.action_ids)
            {
                size_t erased = auth_record.action_ids.erase(action_id);

                if (0 == erased)
                    throw wrong_data_exception(authorization_update.actor + " is not awarded authorization for action id: " + std::to_string(action_id));
            }
        }
    }

    impl.m_authority_manager.set_record(authorization_update.owner, authorization_update.actor, auth_record);
    
    impl.m_authority_manager.smart_cleanup_dummy_record(authorization_update.owner, authorization_update.actor);
}

void action_revert(publiqpp::detail::node_internals& impl,
                   SignedTransaction const& signed_transaction,
                   AuthorizationUpdate const& authorization_update,
                   state_layer layer)
{
    impl.m_authority_manager.smart_create_dummy_record(authorization_update.owner, authorization_update.actor);

    auto auth_record = impl.m_authority_manager.get_record(authorization_update.owner, authorization_update.actor);

    if (authorization_update.update_type == UpdateType::store &&
        authorization_update.action_ids.empty()) // try to reject full authorization
    {
        if (false == auth_record.default_full || false == auth_record.action_ids.empty())
            throw std::logic_error("action_revert: " + authorization_update.actor + " is not awarded full authorizations before rejecting full");

        auth_record.default_full = false;
    }
    else if (authorization_update.update_type == UpdateType::store)
    {
        if (auth_record.default_full)
        {
            for (auto action_id : authorization_update.action_ids)
            {
                auto insert_res = auth_record.action_ids.insert(action_id);

                if (false == insert_res.second)
                    throw std::logic_error("action_revert: " + authorization_update.actor + " is rejected authorization for action id: " + std::to_string(action_id));

                // limitation
                throw std::logic_error("action_revert: impossible to reach state that specifies 'default full' with exceptions");
            }
        }
        else
        {
            for (auto action_id : authorization_update.action_ids)
            {
                size_t erased = auth_record.action_ids.erase(action_id);

                if (0 == erased)
                    throw std::logic_error("action_revert: " + authorization_update.actor + " is not awarded authorization for action id: " + std::to_string(action_id));
            }
        }
    }
    else if (authorization_update.action_ids.empty()) // try to award full authorization
    {
        if (auth_record.default_full || false == auth_record.action_ids.empty())
            throw std::logic_error("action_revert: " + authorization_update.actor + " is not rejected full authorizations before awarding full");

        auth_record.default_full = true;
    }
    else
    {
        if (auth_record.default_full)
        {
            // limitation
            throw std::logic_error("action_revert: parital authorization rejection is not supported");

            // for (auto action_id : authorization_update.action_ids)
            // {
            //     size_t erased = auth_record.action_ids.erase(action_id);

            //     if (0 == erased)
            //         throw std::logic_error("action_revert: " + authorization_update.actor + " is not rejected authorization for action id: " + std::to_string(action_id));
            // }
        }
        else
        {
            for (auto action_id : authorization_update.action_ids)
            {
                auto insert_res = auth_record.action_ids.insert(action_id);

                if (false == insert_res.second)
                    throw std::logic_error("action_revert: " + authorization_update.actor + " is awarded authorization for action id: " + std::to_string(action_id));
            }
        }
    }

    impl.m_authority_manager.set_record(authorization_update.owner, authorization_update.actor, auth_record);
    
    impl.m_authority_manager.smart_cleanup_dummy_record(authorization_update.owner, authorization_update.actor);
}
}