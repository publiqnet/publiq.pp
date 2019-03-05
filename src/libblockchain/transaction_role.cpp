#include "transaction_role.hpp"
#include "common.hpp"
#include "node_internals.hpp"
#include "exception.hpp"

#include <mesh.pp/cryptoutility.hpp>

using namespace BlockchainMessage;

namespace publiqpp
{
void action_validate(SignedTransaction const& signed_transaction,
                     Role const& role)
{
    if (signed_transaction.authority != role.node_address)
        throw authority_exception(signed_transaction.authority, role.node_address);

    if (role.node_type == NodeType::blockchain)
        throw std::runtime_error("there is no need to broadcast role::blockchain");
}

bool action_can_apply(std::unique_ptr<publiqpp::detail::node_internals> const& pimpl,
                      Role const& role)
{
    NodeType node_type;
    if (pimpl->m_state.get_role(role.node_address, node_type))
        return false;
    if (pimpl->m_pb_key.to_string() == role.node_address &&
        pimpl->m_node_type != role.node_type)
        return false;
    return true;
}

void action_apply(std::unique_ptr<publiqpp::detail::node_internals>& pimpl,
                  Role const& role)
{
    NodeType node_type;
    if (pimpl->m_state.get_role(role.node_address, node_type))
        throw std::runtime_error("role: " +
                                 BlockchainMessage::detail::saver(role.node_type) +
                                 " is already stored for: " +
                                 role.node_address);
    if (pimpl->m_pb_key.to_string() == role.node_address &&
        pimpl->m_node_type != role.node_type)
        throw std::runtime_error("the node: " +
                                 role.node_address +
                                 " can have only the following role: " +
                                 BlockchainMessage::detail::saver(pimpl->m_node_type));
    pimpl->m_state.insert_role(role);
}

void action_revert(std::unique_ptr<publiqpp::detail::node_internals>& pimpl,
                   Role const& role)
{
    pimpl->m_state.remove_role(role.node_address);
}
}
