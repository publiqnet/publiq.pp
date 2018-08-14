#pragma once

#include "global.hpp"
#include "message.hpp"
#include "transaction_pool.hpp"

#include <boost/filesystem/path.hpp>

#include <vector>
#include <memory>

namespace publiqpp
{

namespace detail
{
class blockchain_internals;
}

class blockchain
{
public:
    blockchain(boost::filesystem::path const& fs_blockchain);
    ~blockchain();

    void save();
    void commit();
    void discard();

    void update_header();

    uint64_t length() const;
    void header(BlockchainMessage::BlockHeader& block_header) const;

    void insert(BlockchainMessage::SignedBlock const& signed_block);
    void at(uint64_t number, BlockchainMessage::SignedBlock& signed_block) const;
    void header_at(uint64_t number, BlockchainMessage::BlockHeader& block_header) const;
    void remove_last_block();
private:
    std::unique_ptr<detail::blockchain_internals> m_pimpl;
};

}
