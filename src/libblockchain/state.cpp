#include "state.hpp"
#include "common.hpp"
#include "exception.hpp"
#include "node_internals.hpp"
#include "message.tmpl.hpp"
#include "types.hpp"

#include <mesh.pp/fileutility.hpp>

using namespace BlockchainMessage;
namespace filesystem = boost::filesystem;

using std::string;
using std::vector;

namespace publiqpp
{
namespace detail
{

inline
beltpp::void_unique_ptr get_putl_types()
{
    beltpp::message_loader_utility utl;
    StorageTypes::detail::extension_helper(utl);

    auto ptr_utl =
        beltpp::new_void_unique_ptr<beltpp::message_loader_utility>(std::move(utl));

    return ptr_utl;
}

class state_internals
{
public:
    state_internals(filesystem::path const& path,
                    detail::node_internals const& impl)
        : m_accounts("account", path, 10000, detail::get_putl())
        , m_node_accounts("node_account", path, 10000, detail::get_putl())
        , m_roles("role", path, 10, detail::get_putl())
        , m_storages("storages", path, 10000, get_putl_types())
        , pimpl_node(&impl)
    {}

    meshpp::map_loader<Coin> m_accounts;
    meshpp::map_loader<Coin> m_node_accounts;
    meshpp::map_loader<Role> m_roles;
    meshpp::map_loader<StorageTypes::FileUriHolders> m_storages;
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

void state::commit()
{
    m_pimpl->m_accounts.commit();
    m_pimpl->m_node_accounts.commit();
    m_pimpl->m_roles.commit();
}

void state::discard()
{
    m_pimpl->m_accounts.discard();
    m_pimpl->m_node_accounts.discard();
    m_pimpl->m_roles.discard();
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
        m_pimpl->pimpl_node->m_pb_key.to_string() == key)
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

void state::storage_update(std::string const& uri, std::string const& address, UpdateType status)
{
    if (UpdateType::store == status)
    {
        if (false == m_pimpl->m_storages.contains(uri))
        {
            StorageTypes::FileUriHolders holders;
            holders.addresses.insert(address);
            m_pimpl->m_storages.insert(uri, holders);
        }
        else
        {
            StorageTypes::FileUriHolders& holders = m_pimpl->m_storages.at(uri);
            holders.addresses.insert(address);
        }
    }
    else
    {
        if (m_pimpl->m_storages.contains(uri))
        {
            StorageTypes::FileUriHolders& holders = m_pimpl->m_storages.at(uri);
            holders.addresses.erase(address);
        }
    }
}

bool state::storage_has_uri(std::string const& uri, std::string const& address) const
{
    if (false == m_pimpl->m_storages.contains(uri))
        return false;

    StorageTypes::FileUriHolders const& holders = m_pimpl->m_storages.as_const().at(uri);
    return 0 != holders.addresses.count(address);
}

}

