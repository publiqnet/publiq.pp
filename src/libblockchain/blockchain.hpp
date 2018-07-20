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

    void update_consensus_data();

    uint64_t length() const;
    uint64_t consensus_sum() const;
    uint64_t consensus_delta() const;

    void insert(beltpp::packet const& packet);
    bool at(uint64_t number, BlockchainMessage::SignedBlock& signed_block) const;
    void remove_last_block();

    uint64_t calc_delta(std::string key, BlockchainMessage::Block& block, uint64_t amount);
    void mine_block(std::string key, 
                    uint64_t amount,
                    publiqpp::transaction_pool& transaction_pool, 
                    BlockchainMessage::SignedBlock& signed_block);
private:
    std::unique_ptr<detail::blockchain_internals> m_pimpl;
};

}
