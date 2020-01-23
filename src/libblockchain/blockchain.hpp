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
    void commit() noexcept;
    void discard() noexcept;
    void clear();
    void update_state();

    uint64_t length() const;
    std::string last_hash() const;
    BlockchainMessage::BlockHeader const& last_header() const;
    BlockchainMessage::BlockHeaderExtended last_header_ex() const;

    void insert(BlockchainMessage::SignedBlock const& signed_block);
    BlockchainMessage::SignedBlock const& at(uint64_t number) const;
    BlockchainMessage::BlockHeader const& header_at(uint64_t number) const;
    BlockchainMessage::BlockHeaderExtended header_ex_at(uint64_t number) const;
    void remove_last_block();
private:
    std::unique_ptr<detail::blockchain_internals> m_pimpl;
};

}
