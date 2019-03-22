#include "transaction_file.hpp"
#include "common.hpp"
#include "node_internals.hpp"
#include "exception.hpp"

#include <mesh.pp/cryptoutility.hpp>

using namespace BlockchainMessage;

namespace publiqpp
{
void action_validate(SignedTransaction const& signed_transaction,
                     File const& file,
                     bool check_complete)
{
    if (file.author_addresses.empty())
        throw wrong_data_exception("a file has to have at least one author");
    if (check_complete)
    {
        if (signed_transaction.authorizations.size() != file.author_addresses.size())
            throw wrong_data_exception("transaction authorizations error");

        for (size_t index = 0; index != signed_transaction.authorizations.size(); ++index)
        {
            auto const& signer = signed_transaction.authorizations[index].address;
            auto const& author = file.author_addresses[index];

            meshpp::public_key pb_key_author(author);

            if (signer != author)
                throw authority_exception(signer, author);
        }
    }
    else
    {
        if (signed_transaction.authorizations.empty())
            throw wrong_data_exception("transaction authorizations error");

        for (size_t index = 0;
             index != signed_transaction.authorizations.size() &&
             index != file.author_addresses.size();
             ++index)
        {
            auto const& signer = signed_transaction.authorizations[index].address;
            auto const& author = file.author_addresses[index];

            meshpp::public_key pb_key_author(author);

            if (signer != author)
                throw authority_exception(signer, author);
        }
    }

    string file_hash = meshpp::from_base58(file.uri);
    if (file_hash.length() != 32)
        throw std::runtime_error("invalid uri: " + file.uri);
}

authorization_process_result action_authorization_process(SignedTransaction& signed_transaction,
                                                          File const& file)
{
    authorization_process_result code;
    code.complete = (signed_transaction.authorizations.size() == file.author_addresses.size());
    code.modified = false;

    return code;
}

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      File const& file)
{
    if (impl.m_documents.exist_file(file.uri))
        return false;
    return true;
}

void action_apply(publiqpp::detail::node_internals& impl,
                  File const& file)
{
    if (impl.m_documents.exist_file(file.uri))
        throw wrong_document_exception("File already exists!");
    impl.m_documents.insert_file(file);
}

void action_revert(publiqpp::detail::node_internals& impl,
                   File const& file)
{
    impl.m_documents.remove_file(file.uri);
}
}
