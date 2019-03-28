#include "transaction_content.hpp"
#include "common.hpp"
#include "node_internals.hpp"
#include "exception.hpp"

#include <mesh.pp/cryptoutility.hpp>

using namespace BlockchainMessage;

namespace publiqpp
{
void action_validate(SignedTransaction const& signed_transaction,
                     Content const& content,
                     bool /*check_complete*/)
{
    //  this is checked in signed_transaction_validate
    assert(false == signed_transaction.authorizations.empty());

    if (signed_transaction.authorizations.size() != 1)
        throw authority_exception(signed_transaction.authorizations.back().address, string());

    auto signed_authority = signed_transaction.authorizations.front().address;
    if (signed_authority != content.channel_address)
        throw authority_exception(signed_authority, content.channel_address);

    meshpp::public_key pb_key_channel(content.channel_address);

    for (auto const& uri : content.content_unit_uris)
    {
        string unit_hash = meshpp::from_base58(uri);
        if (unit_hash.length() != 32)
            throw std::runtime_error("invalid uri: " + uri);
    }
}

authorization_process_result action_authorization_process(SignedTransaction&/* signed_transaction*/,
                                                          Content const&/* content*/)
{
    authorization_process_result code;
    code.complete = true;
    code.modified = false;

    return code;
}

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      Content const& content)
{
    for (auto const& uri : content.content_unit_uris)
    {
        if (false == impl.m_documents.exist_unit(uri))
            return false;
    }
    return true;
}

void action_apply(publiqpp::detail::node_internals& impl,
                  Content const& content)
{
    for (auto const& uri : content.content_unit_uris)
    {
        if (false == impl.m_documents.exist_unit(uri))
            throw wrong_document_exception("Missing content_unit with uri: " + uri);
    }
}

void action_revert(publiqpp::detail::node_internals& /*impl*/,
                   Content const& /*content*/)
{
}
}
