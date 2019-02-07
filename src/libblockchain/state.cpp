#include "state.hpp"
#include "common.hpp"
#include "exception.hpp"

#include <mesh.pp/fileutility.hpp>

using namespace BlockchainMessage;
namespace filesystem = boost::filesystem;

using std::string;

namespace publiqpp
{
namespace detail
{
class state_internals
{
public:
    state_internals(filesystem::path const& path)
        : m_accounts("account", path, 10000, detail::get_putl())
        , m_contracts("contract", path, 10, detail::get_putl())
        , m_addresses("address", path, 10, detail::get_putl())
    {}

    meshpp::map_loader<Coin> m_accounts;
    meshpp::map_loader<Contract> m_contracts;
    meshpp::map_loader<IPAddress> m_addresses;
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
    m_pimpl->m_contracts.save();
    m_pimpl->m_addresses.save();
}

void state::commit()
{
    m_pimpl->m_accounts.commit();
    m_pimpl->m_contracts.commit();
    m_pimpl->m_addresses.commit();
}

void state::discard()
{
    m_pimpl->m_accounts.discard();
    m_pimpl->m_contracts.discard();
    m_pimpl->m_addresses.discard();
}

Coin state::get_balance(string const& key) const
{
    if (m_pimpl->m_accounts.as_const().contains(key))
        return m_pimpl->m_accounts.as_const().at(key);

    return Coin(); // all accounts not included have 0 balance
}

void state::apply_transfer(Transfer const& transfer, Coin const& fee)
{
    if (coin(transfer.amount).empty())
        throw std::runtime_error("0 amount transfer is not allowed!");

    Coin balance = get_balance(transfer.from);
    if (coin(balance) < transfer.amount + fee)
        throw not_enough_balance_exception(coin(balance), transfer.amount + fee);

    // decrease "from" balance
    decrease_balance(transfer.from, transfer.amount);
    
    // increase "to" balance
    increase_balance(transfer.to, transfer.amount);
}

void state::increase_balance(string const& key, coin const& amount)
{
    if (amount.empty())
        return;

    if (m_pimpl->m_accounts.contains(key))
    {
        Coin& balance = m_pimpl->m_accounts.at(key);
        balance = (balance + amount).to_Coin();
    }
    else
        m_pimpl->m_accounts.insert(key, amount.to_Coin());
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

    balance = coin(balance - amount).to_Coin();

    if (coin(balance).empty())
        m_pimpl->m_accounts.erase(key);
}

void state::get_contracts(std::vector<Contract>& contracts, NodeType const& role) const
{
    contracts.clear();

    for (auto& key : m_pimpl->m_contracts.as_const().keys())
    {
        Contract contract = m_pimpl->m_contracts.as_const().at(key);
        
        if (role == contract.role)
            contracts.push_back(contract);
    }
}

NodeType state::get_contract_type(string const& key) const
{
    if (m_pimpl->m_contracts.as_const().contains(key))
        return m_pimpl->m_contracts.as_const().at(key).role;

    return NodeType::miner;
}

void state::insert_contract(Contract const& contract)
{
    if (contract.role == NodeType::miner)
        throw std::runtime_error("TODO");

    if(get_contract_type(contract.owner) != NodeType::miner)
        throw std::runtime_error("TODO");

    m_pimpl->m_contracts.insert(contract.owner, contract);
}

void state::remove_contract(Contract const& contract)
{
    m_pimpl->m_contracts.erase(contract.owner);
}

void state::update_address(AddressInfo const& address_info)
{
    m_pimpl->m_addresses.insert(address_info.node_id, address_info.ip_address);
}

bool state::get_address(string const& owner, BlockchainMessage::IPAddress& ip_address)
{
    if (m_pimpl->m_addresses.as_const().contains(owner)) // contains is a slow function
    {
        ip_address = m_pimpl->m_addresses.as_const().at(owner);

        return true;
    }

    return false;
}

bool state::get_any_address(BlockchainMessage::AddressInfo& address_info)
{
    // this is just a test code
    auto keys = m_pimpl->m_addresses.as_const().keys();
    if (keys.empty())
        return false;

    auto const& node_id = *keys.begin();
    address_info.node_id = node_id;
    address_info.ip_address = m_pimpl->m_addresses.as_const().at(node_id);

    return true;
}

}

