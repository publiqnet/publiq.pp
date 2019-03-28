#include "transaction_statinfo.hpp"
#include "common.hpp"
#include "node_internals.hpp"
#include "exception.hpp"

#include <mesh.pp/cryptoutility.hpp>

using namespace BlockchainMessage;

namespace publiqpp
{
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

authorization_process_result action_authorization_process(SignedTransaction&/* signed_transaction*/,
                                                          ServiceStatistics const&/* service_statistics*/)
{
    authorization_process_result code;
    code.complete = true;
    code.modified = false;

    return code;
}

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      ServiceStatistics const& service_statistics)
{
    NodeType node_type;
    if (false == impl.m_state.get_role(service_statistics.server_address, node_type) ||
        node_type == NodeType::blockchain)
        return false;

    for (auto const& item : service_statistics.stat_items)
    {
        NodeType item_node_type;
        if (false == impl.m_state.get_role(item.peer_address, item_node_type) ||
            item_node_type == NodeType::blockchain ||
            item_node_type == node_type)
            return false;
    }

    /* change to block number or something ...
    if (stat_info.hash != impl.m_blockchain.last_hash())
        return false;*/

    return true;
}

void action_apply(publiqpp::detail::node_internals& impl,
                  ServiceStatistics const& service_statistics)
{
    NodeType node_type;
    if (false == impl.m_state.get_role(service_statistics.server_address, node_type) ||
        node_type == NodeType::blockchain)
        throw wrong_data_exception("process_stat_info -> wrong authority type : " + service_statistics.server_address);

    for (auto const& item : service_statistics.stat_items)
    {
        NodeType item_node_type;
        if (false == impl.m_state.get_role(item.peer_address, item_node_type) ||
            item_node_type == NodeType::blockchain ||
            item_node_type == node_type)
            throw wrong_data_exception("wrong node type : " + item.peer_address);
    }

    // TODO fix
    //if (stat_info.hash != impl.m_blockchain.last_hash())
    //    throw std::runtime_error("stat_info.hash != impl.m_blockchain.last_hash()");
}

void action_revert(publiqpp::detail::node_internals& /*impl*/,
                   ServiceStatistics const& /*service_statistics*/)
{
}
}
