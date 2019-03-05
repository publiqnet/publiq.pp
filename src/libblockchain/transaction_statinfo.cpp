#include "transaction_statinfo.hpp"
#include "common.hpp"
#include "node_internals.hpp"
#include "exception.hpp"

#include <mesh.pp/cryptoutility.hpp>

using namespace BlockchainMessage;

namespace publiqpp
{
void action_validate(SignedTransaction const& signed_transaction,
                     StatInfo const& stat_info)
{
    if (signed_transaction.authority != stat_info.reporter_address)
        throw authority_exception(signed_transaction.authority, stat_info.reporter_address);
}

bool action_can_apply(std::unique_ptr<publiqpp::detail::node_internals> const& pimpl,
                      StatInfo const& stat_info)
{
    NodeType node_type;
    if (false == pimpl->m_state.get_role(stat_info.reporter_address, node_type) ||
        node_type == NodeType::blockchain)
        return false;

    for (auto const& item : stat_info.items)
    {
        NodeType item_node_type;
        if (false == pimpl->m_state.get_role(item.node_address, item_node_type) ||
            item_node_type == NodeType::blockchain ||
            item_node_type == node_type)
            return false;
    }

    if (stat_info.hash != pimpl->m_blockchain.last_hash())
        return false;

    return true;
}

void action_apply(std::unique_ptr<publiqpp::detail::node_internals>& pimpl,
                  StatInfo const& stat_info)
{
    NodeType node_type;
    if (false == pimpl->m_state.get_role(stat_info.reporter_address, node_type) ||
        node_type == NodeType::blockchain)
        throw wrong_data_exception("process_stat_info -> wrong authority type : " + stat_info.reporter_address);

    for (auto const& item : stat_info.items)
    {
        NodeType item_node_type;
        if (false == pimpl->m_state.get_role(item.node_address, item_node_type) ||
            item_node_type == NodeType::blockchain ||
            item_node_type == node_type)
            throw wrong_data_exception("wrong node type : " + item.node_address);
    }

    if (stat_info.hash != pimpl->m_blockchain.last_hash())
        throw std::runtime_error("stat_info.hash != pimpl->m_blockchain.last_hash()");
}

void action_revert(std::unique_ptr<publiqpp::detail::node_internals>& /*pimpl*/,
                   StatInfo const& /*stat_info*/)
{
}
}
