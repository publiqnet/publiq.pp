#pragma once

#include "global.hpp"
#include "message.hpp"

#include <boost/filesystem/path.hpp>

#include <memory>
#include <vector>

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

    std::vector<std::string> accounts() const;

    void set_balance(std::string const& pb_key, uint64_t amount);
    uint64_t get_balance(std::string const& key) const;

    bool possible_transfer(BlockchainMessage::Transfer const& transfer, uint64_t fee) const;
    void apply_transfer(BlockchainMessage::Transfer const& transfer, uint64_t fee);
private:
    std::unique_ptr<detail::state_internals> m_pimpl;
};
}
