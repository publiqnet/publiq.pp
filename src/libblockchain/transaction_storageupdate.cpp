#include "transaction_storageupdate.hpp"
#include "common.hpp"
#include "node_internals.hpp"
#include "exception.hpp"

#include <mesh.pp/cryptoutility.hpp>

using namespace BlockchainMessage;
using std::string;
using std::vector;

namespace publiqpp
{
vector<string> action_owners(StorageUpdate const& storage_update)
{
    return {storage_update.storage_address};
}
vector<string> action_participants(StorageUpdate const& storage_update)
{
    return {storage_update.storage_address, storage_update.file_uri};
}

void action_validate(SignedTransaction const& signed_transaction,
                     StorageUpdate const& storage_update,
                     bool/* check_complete*/)
{
    //  this is checked in signed_transaction_validate
    assert(false == signed_transaction.authorizations.empty());

    if (signed_transaction.authorizations.size() != 1)
        throw authority_exception(signed_transaction.authorizations.back().address, string());

    meshpp::public_key pb_key_storage_address(storage_update.storage_address);
}

bool action_is_complete(SignedTransaction const&/* signed_transaction*/,
                        StorageUpdate const&/* storage_update*/)
{
    return true;
}

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      SignedTransaction const& signed_transaction,
                      StorageUpdate const& storage_update,
                      state_layer/* layer*/)
{
    assert(signed_transaction.authorizations.size() == 1);

    auto signed_authority = signed_transaction.authorizations.front().address;
    if (false == impl.m_authority_manager.check_authority(storage_update.storage_address, signed_authority, StorageUpdate::rtt))
        return false;

    NodeType node_type;
    if (false == impl.m_state.get_role(storage_update.storage_address, node_type) ||
        node_type != NodeType::storage)
        return false;

    if (false == impl.m_documents.file_exists(storage_update.file_uri))
        return false;

    if (storage_update.status == UpdateType::store &&
        impl.m_documents.storage_has_uri(storage_update.file_uri, storage_update.storage_address))
        return false;
    else if (storage_update.status == UpdateType::remove &&
             false == impl.m_documents.storage_has_uri(storage_update.file_uri, storage_update.storage_address))
        return false;

    return true;
}

void action_apply(publiqpp::detail::node_internals& impl,
                  SignedTransaction const& signed_transaction,
                  StorageUpdate const& storage_update,
                  state_layer/* layer*/)
{
    assert(signed_transaction.authorizations.size() == 1);

    auto signed_authority = signed_transaction.authorizations.front().address;
    if (false == impl.m_authority_manager.check_authority(storage_update.storage_address, signed_authority, StorageUpdate::rtt))
        throw authority_exception(signed_authority, impl.m_authority_manager.get_authority(storage_update.storage_address, StorageUpdate::rtt));

    NodeType node_type;
    if (false == impl.m_state.get_role(storage_update.storage_address, node_type) ||
        node_type != NodeType::storage)
        throw wrong_data_exception("action_apply(StorageUpdate) -> wrong authority type : " + storage_update.storage_address + ", " + storage_update.file_uri);

    if (false == impl.m_documents.file_exists(storage_update.file_uri))
        throw uri_exception(storage_update.file_uri, uri_exception::missing);

    if (storage_update.status == UpdateType::store &&
        impl.m_documents.storage_has_uri(storage_update.file_uri, storage_update.storage_address))
        throw uri_exception(storage_update.file_uri, uri_exception::duplicate);
    else if (storage_update.status == UpdateType::remove &&
             false == impl.m_documents.storage_has_uri(storage_update.file_uri, storage_update.storage_address))
        throw uri_exception(storage_update.file_uri, uri_exception::missing);

    impl.m_documents.storage_update(storage_update.file_uri,
                                    storage_update.storage_address,
                                    storage_update.status);
}

void action_revert(publiqpp::detail::node_internals& impl,
                   SignedTransaction const& signed_transaction,
                   StorageUpdate const& storage_update,
                   state_layer/* layer*/)
{
    impl.m_documents.storage_update(storage_update.file_uri,
                                    storage_update.storage_address,
                                    storage_update.status == UpdateType::remove ? UpdateType::store : UpdateType::remove);

    assert(signed_transaction.authorizations.size() == 1);

    auto signed_authority = signed_transaction.authorizations.front().address;
    if (false == impl.m_authority_manager.check_authority(storage_update.storage_address, signed_authority, StorageUpdate::rtt))
        throw std::logic_error("false == impl.m_authority_manager.check_authority(storage_update.storage_address, signed_authority, StorageUpdate::rtt)");
}
}
