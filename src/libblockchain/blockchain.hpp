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

    void update_state();

    uint64_t length() const;
    std::string last_hash() const;
    void last_header(BlockchainMessage::BlockHeader& block_header) const;

    void insert(BlockchainMessage::SignedBlock const& signed_block);
    BlockchainMessage::SignedBlock const& at(uint64_t number) const;
    void header_at(uint64_t number, BlockchainMessage::BlockHeader& block_header) const;
    void remove_last_block();
private:
    std::unique_ptr<detail::blockchain_internals> m_pimpl;
};

}
