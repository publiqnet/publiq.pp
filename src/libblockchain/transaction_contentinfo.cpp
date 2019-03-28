#include "transaction_contentinfo.hpp"
#include "common.hpp"
#include "node_internals.hpp"
#include "exception.hpp"

#include <mesh.pp/cryptoutility.hpp>

using namespace BlockchainMessage;

namespace publiqpp
{
void action_validate(SignedTransaction const& signed_transaction,
                     StorageUpdate const& storage_update,
                     bool/* check_complete*/)
{
    //  this is checked in signed_transaction_validate
    assert(false == signed_transaction.authorizations.empty());

    if (signed_transaction.authorizations.size() != 1)
        throw authority_exception(signed_transaction.authorizations.back().address, string());

    auto signed_authority = signed_transaction.authorizations.front().address;
    if (signed_authority != storage_update.storage_address)
        throw authority_exception(signed_authority, storage_update.storage_address);
}

authorization_process_result action_authorization_process(SignedTransaction&/* signed_transaction*/,
                                                          StorageUpdate const&/* storage_update*/)
{
    authorization_process_result code;
    code.complete = true;
    code.modified = false;

    return code;
}

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      StorageUpdate const& storage_update)
{
    NodeType node_type;
    if (false == impl.m_state.get_role(storage_update.storage_address, node_type) ||
        node_type != NodeType::storage)
        return false;
    return true;
}

void action_apply(publiqpp::detail::node_internals& impl,
                  StorageUpdate const& storage_update)
{
    NodeType node_type;
    if (false == impl.m_state.get_role(storage_update.storage_address, node_type) ||
        node_type != NodeType::storage)
        throw wrong_data_exception("action_apply(StorageUpdate) -> wrong authority type : " + storage_update.storage_address);
}

void action_revert(publiqpp::detail::node_internals& /*impl*/,
                   StorageUpdate const& /*storage_update*/)
{
}
}
