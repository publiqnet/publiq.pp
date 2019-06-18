#include "transaction_content.hpp"
#include "common.hpp"
#include "node_internals.hpp"
#include "exception.hpp"
#include "sessions.hpp"
#include "message.tmpl.hpp"

#include <mesh.pp/cryptoutility.hpp>

#include <unordered_set>

using namespace BlockchainMessage;
using std::string;
using std::vector;

namespace publiqpp
{
vector<string> action_owners(Content const& content)
{
    return {content.channel_address};
}

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
            throw uri_exception(uri, uri_exception::invalid);
    }
}

bool action_is_complete(SignedTransaction const&/* signed_transaction*/,
                        Content const&/* content*/)
{
    return true;
}

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      Content const& content,
                      state_layer/* layer*/)
{
    NodeType node_type;
    if (false == impl.m_state.get_role(content.channel_address, node_type) ||
        node_type != NodeType::channel)
        throw wrong_data_exception("the content owner is not a channel");

    for (auto const& unit_uri : content.content_unit_uris)
    {
        if (false == impl.m_documents.exist_unit(unit_uri))
            return false;

        ContentUnit const& unit = impl.m_documents.get_unit(unit_uri);
        if (unit.channel_address != content.channel_address ||
            unit.content_id != content.content_id)
            return false;
    }
    return true;
}

void action_apply(publiqpp::detail::node_internals& impl,
                  Content const& content,
                  state_layer layer)
{
    NodeType node_type;
    if (false == impl.m_state.get_role(content.channel_address, node_type) ||
        node_type != NodeType::channel)
        throw wrong_data_exception("the content owner is not a channel");

    for (auto const& unit_uri : content.content_unit_uris)
    {
        if (false == impl.m_documents.exist_unit(unit_uri))
            throw uri_exception(unit_uri, uri_exception::missing);

        ContentUnit const& unit = impl.m_documents.get_unit(unit_uri);
        if (unit.channel_address != content.channel_address ||
            unit.content_id != content.content_id)
            throw wrong_data_exception("the content and the content unit do not correspond to each other");
    }

    if (impl.m_node_type == NodeType::storage &&
        state_layer::chain == layer)
    {
        for (auto const& unit_uri : content.content_unit_uris)
        {
            auto const& unit = impl.m_documents.get_unit(unit_uri);

            for (auto const& file_uri : unit.file_uris)
            {
                auto& value = impl.map_channel_to_file_uris[content.channel_address];
                if (value.count(file_uri))
                    continue;

                value.insert(file_uri);
            }
        }
    }

}

void action_revert(publiqpp::detail::node_internals& /*impl*/,
                   Content const& /*content*/,
                   state_layer/* layer*/)
{
}
}
