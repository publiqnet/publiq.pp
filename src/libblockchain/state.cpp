#include "state.hpp"
#include "common.hpp"
#include "exception.hpp"
#include "node_internals.hpp"
#include "message.tmpl.hpp"

#include <mesh.pp/fileutility.hpp>

using namespace BlockchainMessage;
namespace filesystem = boost::filesystem;

using std::string;
using std::vector;

namespace publiqpp
{
namespace detail
{

class state_internals
{
public:
    state_internals(filesystem::path const& path,
                    detail::node_internals const& impl)
        : m_accounts("account", path, 10000, detail::get_putl())
        , m_node_accounts("node_account", path, 10000, detail::get_putl())
        , m_roles("role", path, 10, detail::get_putl())
        , pimpl_node(&impl)
    {}

    meshpp::map_loader<Coin> m_accounts;
    meshpp::map_loader<Coin> m_node_accounts;
    meshpp::map_loader<Role> m_roles;
    node_internals const* pimpl_node;
};
}

state::state(filesystem::path const& fs_state,
             detail::node_internals const& impl)
    : m_pimpl(new detail::state_internals(fs_state, impl))
{
}

state::~state() = default;

void state::save()
{
    m_pimpl->m_accounts.save();
    m_pimpl->m_node_accounts.save();
    m_pimpl->m_roles.save();
}

void state::commit() noexcept
{
    m_pimpl->m_accounts.commit();
    m_pimpl->m_node_accounts.commit();
    m_pimpl->m_roles.commit();
}

void state::discard() noexcept
{
    m_pimpl->m_accounts.discard();
    m_pimpl->m_node_accounts.discard();
    m_pimpl->m_roles.discard();
}

void state::clear()
{
    m_pimpl->m_accounts.clear();
    m_pimpl->m_node_accounts.clear();
    m_pimpl->m_roles.clear();
}

Coin state::get_balance(string const& key, state_layer layer) const
{
    if (layer == state_layer::pool &&
        m_pimpl->m_accounts.as_const().contains(key))
            return m_pimpl->m_accounts.as_const().at(key);

    if (layer == state_layer::chain &&
        m_pimpl->m_node_accounts.as_const().contains(key))
        return m_pimpl->m_node_accounts.as_const().at(key);

    return Coin(); // all accounts not included have 0 balance
}

void state::set_balance(std::string const& key, coin const& amount, state_layer layer)
{
    Coin Amount;
    amount.to_Coin(Amount);

    if (amount.empty())
        m_pimpl->m_accounts.erase(key);
    else if (m_pimpl->m_accounts.contains(key))
    {
        Coin& balance = m_pimpl->m_accounts.at(key);
        balance = Amount;
    }
    else
        m_pimpl->m_accounts.insert(key, Amount);

    if (state_layer::chain == layer &&
        m_pimpl->pimpl_node->front_public_key().to_string() == key)
    {
        if (amount.empty())
            m_pimpl->m_node_accounts.erase(key);
        else if (m_pimpl->m_node_accounts.contains(key))
        {
            Coin& node_balance = m_pimpl->m_node_accounts.at(key);
            node_balance = Amount;
        }
        else
            m_pimpl->m_node_accounts.insert(key, Amount);
    }
}

void state::increase_balance(string const& key, coin const& amount, state_layer layer)
{
    if (amount.empty())
        return;

    Coin balance = get_balance(key, state_layer::pool);
    set_balance(key, balance + amount, layer);
}

void state::decrease_balance(string const& key, coin const& amount, state_layer layer)
{
    if (amount.empty())
        return;

    Coin balance = get_balance(key, state_layer::pool);

    if (coin(balance) < amount)
        throw not_enough_balance_exception(coin(balance), amount);

    set_balance(key, balance - amount, layer);
}

bool state::get_role(string const& nodeid, NodeType& node_type) const
{
    if (m_pimpl->m_roles.as_const().contains(nodeid))
    {
        node_type = m_pimpl->m_roles.as_const().at(nodeid).node_type;
        return true;
    }

    return false;
}

void state::insert_role(Role const& role)
{
    if (m_pimpl->m_roles.as_const().contains(role.node_address))
        throw std::logic_error("role already exists");

    m_pimpl->m_roles.insert(role.node_address, role);
}

void state::remove_role(string const& nodeid)
{
    m_pimpl->m_roles.erase(nodeid);
}

void state::get_nodes(BlockchainMessage::NodeType const& node_type, vector<string>& nodes) const
{
    nodes.clear();
    unordered_set<string> keys = m_pimpl->m_roles.as_const().keys();

    for (auto node_id : keys)
    {
        NodeType nt;
        if (get_role(node_id, nt) && nt == node_type)
            nodes.push_back(node_id);
    }
}

}

