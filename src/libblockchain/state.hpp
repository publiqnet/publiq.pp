#pragma once

#include "coin.hpp"
#include "message.hpp"

#include <boost/filesystem/path.hpp>

namespace publiqpp
{
namespace detail
{
class state_internals;
}
class state
{
public:
    state(boost::filesystem::path const& fs_state);
    ~state();

    void commit();
    void rollback();

    BlockchainMessage::Coin get_balance(std::string const& key) const;

    bool apply_transfer(BlockchainMessage::Transfer const& transfer, BlockchainMessage::Coin const& fee);

    void increase_balance(std::string const& key, coin const& amount);
    bool decrease_balance(std::string const& key, coin const& amount);

    // for test only
    void apply_reward(BlockchainMessage::Reward const& reward);
private:
    std::unique_ptr<detail::state_internals> m_pimpl;
};
}
