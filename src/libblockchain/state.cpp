#include "state.hpp"
#include "common.hpp"
#include "exception.hpp"

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
    state_internals(filesystem::path const& path)
        : m_accounts("account", path, 10000, detail::get_putl())
        , m_roles("role", path, 10, detail::get_putl())
    {}

    meshpp::map_loader<Coin> m_accounts;
    meshpp::map_loader<Role> m_roles;
};
}

state::state(filesystem::path const& fs_state)
    : m_pimpl(new detail::state_internals(fs_state))
{
}

state::~state() = default;

void state::save()
{
    m_pimpl->m_accounts.save();
    m_pimpl->m_roles.save();
}

void state::commit()
{
    m_pimpl->m_accounts.commit();
    m_pimpl->m_roles.commit();
}

void state::discard()
{
    m_pimpl->m_accounts.discard();
    m_pimpl->m_roles.discard();
}

Coin state::get_balance(string const& key) const
{
    if (m_pimpl->m_accounts.as_const().contains(key))
        return m_pimpl->m_accounts.as_const().at(key);

    return Coin(); // all accounts not included have 0 balance
}

void state::increase_balance(string const& key, coin const& amount)
{
    if (amount.empty())
        return;

    if (m_pimpl->m_accounts.contains(key))
    {
        Coin& balance = m_pimpl->m_accounts.at(key);
        (balance + amount).to_Coin(balance);
    }
    else
    {
        Coin temp;
        amount.to_Coin(temp);
        m_pimpl->m_accounts.insert(key, temp);
    }
}

void state::decrease_balance(string const& key, coin const& amount)
{
    if (amount.empty())
        return;

    if (!m_pimpl->m_accounts.contains(key))
        throw not_enough_balance_exception(coin(), amount);

    Coin& balance = m_pimpl->m_accounts.at(key);

    if (coin(balance) < amount)
        throw not_enough_balance_exception(coin(balance), amount);

    coin(balance - amount).to_Coin(balance);

    if (coin(balance).empty())
        m_pimpl->m_accounts.erase(key);
}

std::vector<std::string> state::get_nodes_by_type(NodeType const& node_type) const
{
    std::vector<std::string> nodeids;

    for (auto& key : m_pimpl->m_roles.as_const().keys())
    {
        Role role = m_pimpl->m_roles.as_const().at(key);
        
        if (node_type == role.node_type)
            nodeids.push_back(role.node_address);
    }

    return nodeids;
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

}

