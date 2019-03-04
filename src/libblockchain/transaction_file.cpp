#include "transaction_file.hpp"
#include "common.hpp"
#include "node_internals.hpp"
#include "exception.hpp"

#include <mesh.pp/cryptoutility.hpp>

using namespace BlockchainMessage;

namespace publiqpp
{
void action_validate(SignedTransaction const& signed_transaction,
                     File const& file)
{
    if (signed_transaction.authority != file.author_address)
        throw authority_exception(signed_transaction.authority, file.author_address);

    meshpp::public_key pb_key_author(file.author_address);

    string file_hash = meshpp::from_base58(file.uri);
    if (file_hash.length() != 32)
        throw std::runtime_error("invalid uri: " + file.uri);
}

bool action_can_apply(std::unique_ptr<publiqpp::detail::node_internals> const& pimpl,
                      File const& file)
{
    if (pimpl->m_documents.exist_file(file.uri))
        return false;
    return true;
}

void action_apply(std::unique_ptr<publiqpp::detail::node_internals>& pimpl,
                  File const& file)
{
    if (pimpl->m_documents.exist_file(file.uri))
        throw wrong_document_exception("File already exists!");
    pimpl->m_documents.insert_file(file);
}

void action_revert(std::unique_ptr<publiqpp::detail::node_internals>& pimpl,
                   File const& file)
{
    pimpl->m_documents.remove_file(file.uri);
}
}
