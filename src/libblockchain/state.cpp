#include "state.hpp"
#include "common.hpp"

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
beltpp::void_unique_ptr get_putl()
{
    beltpp::message_loader_utility utl;
    BlockchainMessage::detail::extension_helper(utl);

    auto ptr_utl =
        beltpp::new_void_unique_ptr<beltpp::message_loader_utility>(std::move(utl));

    return ptr_utl;
}

class state_internals
{
public:
    state_internals(filesystem::path const& path)
        :m_accounts("account", path, detail::get_putl())
    {}

    meshpp::map_loader<Coin> m_accounts;
};
}

state::state(filesystem::path const& fs_state)
    : m_pimpl(new detail::state_internals(fs_state))
{
}

state::~state()
{
}

void state::commit()
{
    m_pimpl->m_accounts.save();
}

void state::rollback()
{
    m_pimpl->m_accounts.discard();
}

Coin state::get_balance(string const& key) const
{
    if (m_pimpl->m_accounts.contains(key))
        return m_pimpl->m_accounts.at(key);

    return Coin(); // all accounts not included have 0 balance
}

void state::apply_transfer(Transfer const& transfer, Coin const& fee)
{
    if (coin(transfer.amount).empty())
        throw std::runtime_error("0 amount transfer is not allowed!");

    // decrease "from" balance
    decrease_balance(transfer.from, transfer.amount + fee);
    
    // increase "to" balance
    increase_balance(transfer.to, transfer.amount);
}

void state::apply_reward(Reward const& reward)
{
    if (coin(reward.amount).empty())
        throw std::runtime_error("0 amount reward is humiliatingly!");

    increase_balance(reward.to, reward.amount);
}

void state::increase_balance(string const& key, coin const& amount)
{
    if (amount.empty())
        return;

    if (m_pimpl->m_accounts.contains(key))
    {
        coin balance = m_pimpl->m_accounts.at(key);
        balance += amount;
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

    coin balance = m_pimpl->m_accounts.at(key);

    if (balance < amount)
        throw low_balance_exception(key);

    balance -= amount;

    if (balance.empty())
        m_pimpl->m_accounts.erase(key);
}

}

//---------------- Exceptions -----------------------
low_balance_exception::low_balance_exception(string const& _key)
    : runtime_error("Account:" + _key + " balance is not enough!")
    , key(_key)
{}
low_balance_exception::low_balance_exception(low_balance_exception const& other) noexcept
    : runtime_error(other)
    , key(other.key)
{}
low_balance_exception& low_balance_exception::operator=(low_balance_exception const& other) noexcept
{
    dynamic_cast<runtime_error*>(this)->operator =(other);
    key = other.key;
    return *this;
}
low_balance_exception::~low_balance_exception() noexcept
{}

