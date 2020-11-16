#include "transaction_file.hpp"
#include "common.hpp"
#include "node_internals.hpp"
#include "exception.hpp"

#include <mesh.pp/cryptoutility.hpp>

#include <unordered_set>

using namespace BlockchainMessage;
using std::string;
using std::vector;
using std::unordered_set;

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
    }
    else
    {
        if (signed_transaction.authorizations.size() != 1)
            throw authority_exception(signed_transaction.authorizations[1].address, string());
    }
    
    unordered_set<string> set_authors;
    for (auto const& author : file.author_addresses)
    {
        meshpp::public_key pb_key_author(author);

        auto insert_res = set_authors.insert(author);
        if (false == insert_res.second)
            throw wrong_data_exception("duplicate author: " + author);
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
                      SignedTransaction const& signed_transaction,
                      File const& file,
                      state_layer/* layer*/)
{
    if (action_is_complete(signed_transaction, file))
    {
        for (size_t index = 0; index != signed_transaction.authorizations.size(); ++index)
        {
            auto const& signer = signed_transaction.authorizations[index].address;
            auto const& author = file.author_addresses[index];

            if (false == impl.m_authority_manager.check_authority(author, signer, File::rtt))
                return false;
        }
    }

    if (impl.m_documents.file_exists(file.uri))
        return false;
        
    return true;
}

void action_apply(publiqpp::detail::node_internals& impl,
                  SignedTransaction const& signed_transaction,
                  File const& file,
                  state_layer/* layer*/)
{
    if (action_is_complete(signed_transaction, file))
    {
        for (size_t index = 0; index != signed_transaction.authorizations.size(); ++index)
        {
            auto const& signer = signed_transaction.authorizations[index].address;
            auto const& author = file.author_addresses[index];

            if (false == impl.m_authority_manager.check_authority(author, signer, File::rtt))
                throw authority_exception(signer, impl.m_authority_manager.get_authority(author, File::rtt));
        }
    }

    if (impl.m_documents.file_exists(file.uri))
        throw uri_exception(file.uri, uri_exception::duplicate);

    impl.m_documents.insert_file(file);
}

void action_revert(publiqpp::detail::node_internals& impl,
                   SignedTransaction const& signed_transaction,
                   File const& file,
                   state_layer/* layer*/)
{
    impl.m_documents.remove_file(file.uri);

    for (size_t index = 0; index != signed_transaction.authorizations.size(); ++index)
    {
        auto const& signer = signed_transaction.authorizations[index].address;
        auto const& author = file.author_addresses[index];

        if (false == impl.m_authority_manager.check_authority(author, signer, File::rtt))
            throw std::logic_error("false == impl.m_authority_manager.check_authority(author, signer, File::rtt)");
    }
}
}
