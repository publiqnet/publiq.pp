#pragma once

#include "global.hpp"
#include "message.hpp"

#include <belt.pp/packet.hpp>

#include <boost/filesystem/path.hpp>

#include <vector>
#include <memory>
#include <string>

namespace publiqpp
{

namespace detail
{
class transaction_pool_internals;
}

class transaction_pool
{
public:
    transaction_pool(boost::filesystem::path const& fs_transaction_pool);
    ~transaction_pool();

    size_t length() const;
    bool insert(beltpp::packet const& package);
    bool at(std::string const& key, BlockchainMessage::SignedTransaction& signed_transaction) const;
    bool remove(std::string const& key);

    bool contains(std::string const& key) const;
    void get_keys(std::vector<std::string> &keys) const;
    void get_amounts(std::string const& key, 
                     std::vector<std::pair<std::string, uint64_t>>& amounts, 
                     bool in_out) const;

    void grant_rewards(std::vector<BlockchainMessage::Reward>& rewards) const;
private:
    std::unique_ptr<detail::transaction_pool_internals> m_pimpl;
};

}
