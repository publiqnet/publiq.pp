#pragma once

#include "global.hpp"
#include "message.hpp"
#include "transaction_pool.hpp"

#include <boost/filesystem/path.hpp>

#include <mesh.pp/cryptoutility.hpp>

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

    void update_header();

    uint64_t length() const;
    void header(BlockchainMessage::BlockHeader& block_header) const;

    bool insert(beltpp::packet const& packet);
    bool at(uint64_t number, BlockchainMessage::SignedBlock& signed_block) const;
    bool header_at(uint64_t number, BlockchainMessage::BlockHeader& block_header) const;
    void remove_last_block();

    uint64_t calc_delta(std::string const& key, uint64_t amount, BlockchainMessage::Block const& block) const;
    void mine_block(meshpp::private_key const& pv_key, uint64_t amount, 
                    publiqpp::transaction_pool const& transaction_pool);
    
    bool tmp_block(BlockchainMessage::SignedBlock& signed_block) const;
    bool tmp_header(BlockchainMessage::BlockHeader& block_header) const;
    void tmp_keys(std::vector<std::string> &keys) const;

    void step_disable();
    bool mine_allowed() const;
    bool apply_allowed() const;

private:
    std::unique_ptr<detail::blockchain_internals> m_pimpl;
};

}
