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

bool state::apply_transfer(BlockchainMessage::Transfer const& transfer, BlockchainMessage::Coin const& fee)
{
    if (coin(transfer.amount).empty())
        return false;

    // decrease "from" balance
    if (m_pimpl->m_state.keys.find(transfer.from) != m_pimpl->m_state.keys.end())
    {
        coin balance = m_pimpl->m_state.at(transfer.from);

        if (balance >= transfer.amount + fee)
            balance -= transfer.amount + fee;
        else
            return false;
    }
    
    // increase "to" balance
    if (m_pimpl->m_state.keys.find(transfer.to) != m_pimpl->m_state.keys.end())
    {
        coin balance = m_pimpl->m_state.at(transfer.to);
        balance += transfer.amount;
    }
    else
        m_pimpl->m_state.insert(transfer.to, transfer.amount);

    return true;
}

void state::apply_reward(BlockchainMessage::Reward const& reward)
{
    if (coin(reward.amount).empty())
        throw std::runtime_error("0 amount reward is humiliatingly!");

    if (m_pimpl->m_state.keys.find(reward.to) != m_pimpl->m_state.keys.end())
    {
        coin balance = m_pimpl->m_state.at(reward.to);
        balance += reward.amount;
    }
    else
        m_pimpl->m_state.insert(reward.to, reward.amount);
}

void state::merge_block(std::unordered_map<string, BlockchainMessage::Coin> const& tmp_state)
{
    //for (auto &it : tmp_state)
    //{
    //    if (coin(it.second).empty())
    //        m_pimpl->m_state->accounts.erase(it.first);
    //    else
    //        m_pimpl->m_state->accounts[it.first] = it.second;
    //}
    //
    //// save state to file after merge complete
    //m_pimpl->m_state.save();
}

}
