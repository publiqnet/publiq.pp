#include "transaction_role.hpp"
#include "common.hpp"
#include "node_internals.hpp"
#include "exception.hpp"

#include <mesh.pp/cryptoutility.hpp>

using namespace BlockchainMessage;
using std::string;
using std::vector;

namespace publiqpp
{
vector<string> action_owners(Role const& role)
{
    return {role.node_address};
}
vector<string> action_participants(Role const& role)
{
    return {role.node_address};
}

void action_validate(SignedTransaction const& signed_transaction,
                     Role const& role,
                     bool/* check_complete*/)
{
    //  this is checked in signed_transaction_validate
    assert(false == signed_transaction.authorizations.empty());

    if (signed_transaction.authorizations.size() != 1)
        throw authority_exception(signed_transaction.authorizations.back().address, string());

    auto signed_authority = signed_transaction.authorizations.front().address;
    if (signed_authority != role.node_address)
        throw authority_exception(signed_authority, role.node_address);

    if (role.node_type == NodeType::blockchain)
        throw std::runtime_error("there is no need to broadcast role::blockchain");
}

bool action_is_complete(SignedTransaction const&/* signed_transaction*/,
                        Role const&/* role*/)
{
    return true;
}

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      SignedTransaction const&/* signed_transaction*/,
                      Role const& role,
                      state_layer/* layer*/)
{
    NodeType node_type;
    if (impl.m_state.get_role(role.node_address, node_type))
        return false;
    if (impl.m_pb_key.to_string() == role.node_address &&
        impl.m_node_type != role.node_type)
        return false;
    return true;
}

void action_apply(publiqpp::detail::node_internals& impl,
                  SignedTransaction const&/* signed_transaction*/,
                  Role const& role,
                  state_layer/* layer*/)
{
    NodeType node_type;
    if (impl.m_state.get_role(role.node_address, node_type))
        throw std::runtime_error("role: " +
                                 BlockchainMessage::to_string(role.node_type) +
                                 " is already stored for: " +
                                 role.node_address);
    if (impl.m_pb_key.to_string() == role.node_address &&
        impl.m_node_type != role.node_type)
        throw std::runtime_error("the node: " +
                                 role.node_address +
                                 " can have only the following role: " +
                                 BlockchainMessage::to_string(impl.m_node_type));
    impl.m_state.insert_role(role);
}

void action_revert(publiqpp::detail::node_internals& impl,
                   SignedTransaction const&/* signed_transaction*/,
                   Role const& role,
                   state_layer/* layer*/)
{
    impl.m_state.remove_role(role.node_address);
}
}
