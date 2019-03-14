#include "transaction_contentunit.hpp"
#include "common.hpp"
#include "node_internals.hpp"
#include "exception.hpp"

#include <mesh.pp/cryptoutility.hpp>

using namespace BlockchainMessage;

namespace publiqpp
{
void action_validate(SignedTransaction const& signed_transaction,
                     ContentUnit const& content_unit)
{
    if (signed_transaction.authority != content_unit.author_address)
        throw authority_exception(signed_transaction.authority, content_unit.author_address);

    meshpp::public_key pb_key_author(content_unit.author_address);
    meshpp::public_key pb_key_channel(content_unit.channel_address);

    string unit_hash = meshpp::from_base58(content_unit.uri);
    if (unit_hash.length() != 32)
        throw std::runtime_error("invalid uri: " + content_unit.uri);

    for (auto const& file_uri : content_unit.file_uris)
    {
        string file_hash = meshpp::from_base58(file_uri);
        if (file_hash.length() != 32)
            throw std::runtime_error("invalid uri: " + file_uri);
    }
}

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      ContentUnit const& content_unit)
{
    if (impl.m_documents.exist_unit(content_unit.uri))
        return false;

    for (auto const& uri : content_unit.file_uris)
    {
        if (false == impl.m_documents.exist_file(uri))
            return false;
    }
    return true;
}

void action_apply(publiqpp::detail::node_internals& impl,
                  ContentUnit const& content_unit)
{
    if (impl.m_documents.exist_unit(content_unit.uri))
        throw wrong_document_exception("ContentUnit already exists: " + content_unit.uri);

    for (auto const& uri : content_unit.file_uris)
    {
        if (false == impl.m_documents.exist_file(uri))
            throw wrong_document_exception("Missing File: " + uri);
    }

    impl.m_documents.insert_unit(content_unit);
}

void action_revert(publiqpp::detail::node_internals& impl,
                   ContentUnit const& content_unit)
{
    impl.m_documents.remove_unit(content_unit.uri);
}
}
