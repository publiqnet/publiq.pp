#include "transaction_contentunit.hpp"
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
vector<string> action_owners(ContentUnit const& content_unit)
{
    return content_unit.author_addresses;
}
vector<string> action_participants(ContentUnit const& content_unit)
{
    vector<string> result = content_unit.author_addresses;
    result.push_back(content_unit.uri);
    result.insert(result.end(), content_unit.file_uris.begin(), content_unit.file_uris.end());
    result.push_back(content_unit.channel_address);
    return result;
}

void action_validate(SignedTransaction const& signed_transaction,
                     ContentUnit const& content_unit,
                     bool check_complete)
{
    //  this is checked in signed_transaction_validate
    assert(false == signed_transaction.authorizations.empty());

    if (content_unit.author_addresses.empty())
        throw wrong_data_exception("a content unit has to have at least one author");

    if (check_complete)
    {
        if (signed_transaction.authorizations.size() > content_unit.author_addresses.size())
            throw authority_exception(signed_transaction.authorizations.back().address, string());
        else if (signed_transaction.authorizations.size() < content_unit.author_addresses.size())
            throw authority_exception(string(), content_unit.author_addresses.back());

        for (size_t index = 0; index != signed_transaction.authorizations.size(); ++index)
        {
            auto const& signer = signed_transaction.authorizations[index].address;
            auto const& author = content_unit.author_addresses[index];

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
        for (size_t index = 0; index != content_unit.author_addresses.size(); ++index)
        {
            auto const& signer = signed_transaction.authorizations.front().address;
            auto const& author = content_unit.author_addresses[index];

            meshpp::public_key pb_key_author(author);

            if (signer == author)
                found = true;
        }

        if (false == found)
            throw authority_exception(signed_transaction.authorizations.front().address, string());
    }
    meshpp::public_key pb_key_channel(content_unit.channel_address);

    if (content_unit.file_uris.empty())
        throw uri_exception(string(), uri_exception::missing);

    string unit_hash = meshpp::from_base58(content_unit.uri);
    if (unit_hash.length() != 32)
        throw uri_exception(content_unit.uri, uri_exception::invalid);

    for (auto const& file_uri : content_unit.file_uris)
    {
        string file_hash = meshpp::from_base58(file_uri);
        if (file_hash.length() != 32)
            throw uri_exception(file_uri, uri_exception::invalid);
    }
}

bool action_is_complete(SignedTransaction const& signed_transaction,
                        ContentUnit const& content_unit)
{
    return (signed_transaction.authorizations.size() == content_unit.author_addresses.size());
}

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      SignedTransaction const&/* signed_transaction*/,
                      ContentUnit const& content_unit,
                      state_layer/* layer*/)
{

    string duplicate_file_uri;
    if (false == impl.pcontent_unit_validate_check(content_unit.file_uris,
                                                   duplicate_file_uri,
                                                   impl.m_blockchain.length(),
                                                   impl.m_testnet))
        return false;

    if (impl.m_documents.unit_exists(content_unit.uri))
        return false;

    unordered_set<string> file_uris;
    for (auto const& uri : content_unit.file_uris)
        file_uris.insert(uri);

    return impl.m_documents.files_exist(file_uris).first;
}

void action_apply(publiqpp::detail::node_internals& impl,
                  SignedTransaction const&/* signed_transaction*/,
                  ContentUnit const& content_unit,
                  state_layer/* layer*/)
{

    string duplicate_file_uri;
    if (false == impl.pcontent_unit_validate_check(content_unit.file_uris,
                                                   duplicate_file_uri,
                                                   impl.m_blockchain.length(),
                                                   impl.m_testnet))
        throw uri_exception(duplicate_file_uri, uri_exception::duplicate);
    /*
    unordered_set<string> file_uris;
    for (auto const& file_uri : content_unit.file_uris)
    {
        auto insert_res = file_uris.insert(file_uri);
        if (false == insert_res.second)
            throw uri_exception(file_uri, uri_exception::duplicate);
    }
    */

    if (impl.m_documents.unit_exists(content_unit.uri))
        throw uri_exception(content_unit.uri, uri_exception::duplicate);

    unordered_set<string> file_uris;
    for (auto const& uri : content_unit.file_uris)
        file_uris.insert(uri);

    auto check = impl.m_documents.files_exist(file_uris);
    if (false == check.first)
        throw uri_exception(check.second, uri_exception::missing);

    impl.m_documents.insert_unit(content_unit);
}

void action_revert(publiqpp::detail::node_internals& impl,
                   SignedTransaction const&/* signed_transaction*/,
                   ContentUnit const& content_unit,
                   state_layer/* layer*/)
{
    impl.m_documents.remove_unit(content_unit.uri);
}
}
