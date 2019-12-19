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
class node_internals;
}

class transaction_pool
{
public:
    transaction_pool(boost::filesystem::path const& fs_transaction_pool);
    ~transaction_pool();

    void save();
    void commit();
    void discard();
    void clear();

    size_t length() const;
    void push_back(BlockchainMessage::SignedTransaction const& signed_transaction);
    void pop_back();
    BlockchainMessage::SignedTransaction const& at(size_t index) const;
    BlockchainMessage::SignedTransaction& ref_at(size_t index) const;

private:
    std::unique_ptr<detail::transaction_pool_internals> m_pimpl;
};

void load_transaction_cache(publiqpp::detail::node_internals& impl);

}
