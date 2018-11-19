#include "state.hpp"
#include "common.hpp"

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
        :m_accounts("account", path, 10000, detail::get_putl())
        ,m_contracts("contract", path, 10, detail::get_putl())
    {}

    meshpp::map_loader<Coin> m_accounts;
    meshpp::map_loader<Contract> m_contracts;
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
}

void state::commit()
{
    m_pimpl->m_accounts.commit();
    m_pimpl->m_contracts.commit();
}

void state::discard()
{
    m_pimpl->m_accounts.discard();
    m_pimpl->m_contracts.discard();
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
        throw low_balance_exception(transfer.from);

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
        throw low_balance_exception(key);

    Coin& balance = m_pimpl->m_accounts.at(key);

    if (coin(balance) < amount)
        throw low_balance_exception(key);

    balance = coin(balance - amount).to_Coin();

    if (coin(balance).empty())
        m_pimpl->m_accounts.erase(key);
}

void state::get_contracts(std::vector<Contract>& contracts, uint64_t const& type) const
{//TODO
    contracts.clear();

    for (auto& key : m_pimpl->m_contracts.as_const().keys())
    {
        Contract contract = m_pimpl->m_contracts.as_const().at(key);

        if (type == 0 || type == contract.type)
            contracts.push_back(contract);
    }
}

uint64_t state::get_contract_type(string const& key) const
{//TODO
    if (m_pimpl->m_contracts.as_const().contains(key))
        return m_pimpl->m_contracts.as_const().at(key).type;

    return 0;
}

void state::insert_contract(Contract const& contract)
{
    if (contract.type == 0)
        throw std::runtime_error("TODO");

    m_pimpl->m_contracts.insert(contract.owner, contract);
}

void state::remove_contract(Contract const& contract)
{
    m_pimpl->m_contracts.erase(contract.owner);
}

}

//---------------- Exceptions -----------------------
low_balance_exception::low_balance_exception(string const& _account)
    : runtime_error("Low balance! account: " + _account)
    , account(_account)
{}
low_balance_exception::low_balance_exception(low_balance_exception const& other) noexcept
    : runtime_error(other)
    , account(other.account)
{}
low_balance_exception& low_balance_exception::operator=(low_balance_exception const& other) noexcept
{
    dynamic_cast<runtime_error*>(this)->operator =(other);
    account = other.account;
    return *this;
}
low_balance_exception::~low_balance_exception() noexcept
{}

