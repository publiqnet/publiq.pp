#pragma once

#include "global.hpp"
#include "common.hpp"

#include <belt.pp/packet.hpp>

#include <boost/filesystem/path.hpp>

#include <vector>
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

    void save();
    void commit();
    void discard();

    size_t length() const;
    bool contains(std::string const& key) const;
    void insert(BlockchainMessage::SignedTransaction const& signed_transaction);
    void at(std::string const& key, BlockchainMessage::SignedTransaction& signed_transaction) const;
    void remove(std::string const& key);
    void get_keys(std::vector<std::string> &keys) const;

private:
    std::unique_ptr<detail::transaction_pool_internals> m_pimpl;
};

}
