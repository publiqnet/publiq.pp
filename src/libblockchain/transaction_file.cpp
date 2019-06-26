#include "transaction_file.hpp"
#include "common.hpp"
#include "node_internals.hpp"
#include "exception.hpp"

#include <mesh.pp/cryptoutility.hpp>

using namespace BlockchainMessage;
using std::string;
using std::vector;

namespace publiqpp
{
vector<string> action_owners(File const& file)
{
    return file.author_addresses;
}
vector<string> action_participants(File const& file)
{
    vector<string> result = file.author_addresses;
    result.push_back(file.uri);

    return result;
}

void action_validate(SignedTransaction const& signed_transaction,
                     File const& file,
                     bool check_complete)
{
    //  this is checked in signed_transaction_validate
    assert(false == signed_transaction.authorizations.empty());

    if (file.author_addresses.empty())
        throw wrong_data_exception("a file has to have at least one author");

    if (check_complete)
    {
        if (signed_transaction.authorizations.size() > file.author_addresses.size())
            throw authority_exception(signed_transaction.authorizations.back().address, string());
        else if (signed_transaction.authorizations.size() < file.author_addresses.size())
            throw authority_exception(string(), file.author_addresses.back());

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
        if (signed_transaction.authorizations.size() != 1)
            throw authority_exception(signed_transaction.authorizations[1].address, string());

        bool found = false;
        for (size_t index = 0; index != file.author_addresses.size(); ++index)
        {
            auto const& signer = signed_transaction.authorizations.front().address;
            auto const& author = file.author_addresses[index];

            meshpp::public_key pb_key_author(author);

            if (signer == author)
                found = true;
        }

        if (false == found)
            throw authority_exception(signed_transaction.authorizations.front().address, string());
    }

    string file_hash = meshpp::from_base58(file.uri);
    if (file_hash.length() != 32)
        throw uri_exception(file.uri, uri_exception::invalid);
}

bool action_is_complete(SignedTransaction const& signed_transaction,
                        File const& file)
{
    return (signed_transaction.authorizations.size() == file.author_addresses.size());
}

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      File const& file,
                      state_layer/* layer*/)
{
    if (impl.m_documents.exist_file(file.uri))
        return false;
    return true;
}

void action_apply(publiqpp::detail::node_internals& impl,
                  File const& file,
                  state_layer/* layer*/)
{
    if (impl.m_documents.exist_file(file.uri))
        throw uri_exception(file.uri, uri_exception::duplicate);
    impl.m_documents.insert_file(file);
}

void action_revert(publiqpp::detail::node_internals& impl,
                   File const& file,
                   state_layer/* layer*/)
{
    impl.m_documents.remove_file(file.uri);
}
}
