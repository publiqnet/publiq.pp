#include "transaction_statinfo.hpp"
#include "common.hpp"
#include "node_internals.hpp"
#include "exception.hpp"

#include <mesh.pp/cryptoutility.hpp>

using namespace BlockchainMessage;
using std::string;
using std::vector;

namespace publiqpp
{
vector<string> action_owners(ServiceStatistics const& service_statistics)
{
    return {service_statistics.server_address};
}

void action_validate(SignedTransaction const& signed_transaction,
                     ServiceStatistics const& service_statistics,
                     bool/* check_complete*/)
{
    //  this is checked in signed_transaction_validate
    assert(false == signed_transaction.authorizations.empty());

    if (signed_transaction.authorizations.size() != 1)
        throw authority_exception(signed_transaction.authorizations.back().address, string());

    auto signed_authority = signed_transaction.authorizations.front().address;
    if (signed_authority != service_statistics.server_address)
        throw authority_exception(signed_authority, service_statistics.server_address);
}

bool action_is_complete(SignedTransaction const&/* signed_transaction*/,
                        ServiceStatistics const&/* service_statistics*/)
{
    return true;
}

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      ServiceStatistics const& service_statistics)
{
    NodeType node_type;
    if (false == impl.m_state.get_role(service_statistics.server_address, node_type) ||
        node_type == NodeType::blockchain)
        return false;

    for (auto const& item : service_statistics.file_items)
    {
        if (false == impl.m_documents.exist_file(item.file_uri))
            return false;

        if (node_type == NodeType::channel &&
            false == impl.m_documents.exist_unit(item.unit_uri))
            return false;

        for (auto const& it : item.count_items)
        {
            NodeType item_node_type;
            if (false == impl.m_state.get_role(it.peer_address, item_node_type) ||
                item_node_type == NodeType::blockchain ||
                item_node_type == node_type)
                return false;
        }
    }

    return true;
}

void action_apply(publiqpp::detail::node_internals& impl,
                  ServiceStatistics const& service_statistics,
                  state_layer/* layer*/)
{
    NodeType node_type;
    if (false == impl.m_state.get_role(service_statistics.server_address, node_type) ||
        node_type == NodeType::blockchain)
        throw wrong_data_exception("process_stat_info -> wrong authority type : " + service_statistics.server_address);

    for (auto const& item : service_statistics.file_items)
    {
        if (false == impl.m_documents.exist_file(item.file_uri))
            throw uri_exception(item.file_uri, uri_exception::missing);

        if (node_type == NodeType::channel &&
            false == impl.m_documents.exist_unit(item.unit_uri))
            throw uri_exception(item.unit_uri, uri_exception::missing);

        for (auto const& it : item.count_items)
        {
            NodeType item_node_type;
            if (false == impl.m_state.get_role(it.peer_address, item_node_type) ||
                item_node_type == NodeType::blockchain ||
                item_node_type == node_type)
                throw wrong_data_exception("wrong node type : " + it.peer_address);
        }
    }
}

void action_revert(publiqpp::detail::node_internals& /*impl*/,
                   ServiceStatistics const& /*service_statistics*/,
                   state_layer/* layer*/)
{
}
}
