#pragma once

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

    uint64_t get_balance(std::string const& key) const;

    bool check_transfer(BlockchainMessage::Transfer const& transfer, uint64_t fee) const;
    void apply_transfer(BlockchainMessage::Transfer const& transfer, uint64_t fee);
    void merge_block(std::unordered_map<std::string, uint64_t> const& tmp_state);

    // for test only
    void state::apply_reward(BlockchainMessage::Reward const& reward);
private:
    std::unique_ptr<detail::state_internals> m_pimpl;
};
}
