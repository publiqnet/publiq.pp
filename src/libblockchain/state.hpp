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

    void apply_transfer(BlockchainMessage::Transfer const& transfer, BlockchainMessage::Coin const& fee);

    void increase_balance(std::string const& key, coin const& amount);
    void decrease_balance(std::string const& key, coin const& amount);

    // for test only
    void apply_reward(BlockchainMessage::Reward const& reward);
private:
    std::unique_ptr<detail::state_internals> m_pimpl;
};
}

//---------------- Exceptions -----------------------
class low_balance_exception : public std::runtime_error
{
public:
    explicit low_balance_exception(std::string const& key);

    low_balance_exception(low_balance_exception const&) noexcept;
    low_balance_exception& operator=(low_balance_exception const&) noexcept;

    virtual ~low_balance_exception() noexcept;

    std::string key;
};
