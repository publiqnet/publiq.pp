#include "state.hpp"
#include "common.hpp"

#include <mesh.pp/fileutility.hpp>

using namespace BlockchainMessage;
namespace filesystem = boost::filesystem;
using std::string;
using std::vector;

using state_data_loader = meshpp::file_loader<StateFileData,
                                              &StateFileData::from_string,
                                              &StateFileData::to_string>;

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
        :m_state("balance", path, detail::get_putl())
    {}

    meshpp::map_loader<Coin> m_state;
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
    m_pimpl->m_state.save();
}

void state::rollback()
{
    m_pimpl->m_state.discard();
}

Coin state::get_balance(string const& key) const
{
    if (m_pimpl->m_state.keys.find(key) != m_pimpl->m_state.keys.end())
        return m_pimpl->m_state.at(key);

    return Coin(); // all accounts not included have 0 balance
}

bool state::apply_transfer(Transfer const& transfer, Coin const& fee)
{
    if (coin(transfer.amount).empty())
        throw std::runtime_error("0 amount transfer is not allowed!");

    // decrease "from" balance
    if (!decrease_balance(transfer.from, transfer.amount + fee))
        return false;
    
    // increase "to" balance
    increase_balance(transfer.to, transfer.amount);

    return true;
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
        throw std::runtime_error("Increment balance with 0 is not allowed!");

    if (m_pimpl->m_state.keys.find(key) != m_pimpl->m_state.keys.end())
    {
        coin balance = m_pimpl->m_state.at(key);
        balance += amount;
    }
    else
        m_pimpl->m_state.insert(key, amount.to_Coin());
}

bool state::decrease_balance(string const& key, coin const& amount)
{
    if (amount.empty())
        throw std::runtime_error("Decrement balance with 0 is not allowed!");

    if (m_pimpl->m_state.keys.find(key) == m_pimpl->m_state.keys.end())
        return false;

    coin balance = m_pimpl->m_state.at(key);

    if (balance < amount)
        return false;

    balance -= amount;

    if (balance.empty())
        m_pimpl->m_state.erase(key);

    return true;
}

}
