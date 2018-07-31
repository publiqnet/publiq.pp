#include "state.hpp"

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

class state_internals
{
public:
    state_internals(filesystem::path const& path)
        : m_path(path)
        , m_state(path / "accounts.state")
    {}

    filesystem::path m_path;
    state_data_loader m_state;
};
}

state::state(filesystem::path const& fs_state)
    : m_pimpl(new detail::state_internals(fs_state))
{
}

state::~state()
{
}

uint64_t state::get_balance(std::string const& key) const
{
    if (m_pimpl->m_state->accounts.find(key) != m_pimpl->m_state->accounts.end())
        return m_pimpl->m_state->accounts[key];

    return 0; // all accounts not included have 0 balance
}

bool state::check_transfer(BlockchainMessage::Transfer const& transfer, uint64_t fee) const
{
    if (transfer.amount == 0)
        throw std::runtime_error("0 amount transfer is restricted!");

    if (m_pimpl->m_state->accounts.find(transfer.from) != m_pimpl->m_state->accounts.end())
    {
        uint64_t balance = m_pimpl->m_state->accounts[transfer.from];

        if (balance >= transfer.amount + fee)
            return true;
    }

    return false; // all accounts not included have 0 balance
}

void state::apply_transfer(BlockchainMessage::Transfer const& transfer, uint64_t fee)
{
    if (!check_transfer(transfer, fee))
        throw std::runtime_error("Transfer balance is not enough!");

    // decrease "from" balance
    uint64_t balance = m_pimpl->m_state->accounts[transfer.from];

    // balance is checked above
    balance = balance - transfer.amount - fee;

    if (balance == 0)
        m_pimpl->m_state->accounts.erase(transfer.from);
    else
        m_pimpl->m_state->accounts[transfer.from] = balance;
    
    // increase "to" balance
    balance = m_pimpl->m_state->accounts[transfer.to];
    m_pimpl->m_state->accounts[transfer.to] = balance + transfer.amount;

    // save state to file after each change
    m_pimpl->m_state.save();
}

void state::merge_block(std::unordered_map<string, uint64_t> const& tmp_state)
{
    for (auto &it : tmp_state)
    {
        if (it.second == 0)
            m_pimpl->m_state->accounts.erase(it.first);
        else
            m_pimpl->m_state->accounts[it.first] = it.second;
    }
}

}
