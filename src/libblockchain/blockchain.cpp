#include "blockchain.hpp"
#include "common.hpp"

#include <belt.pp/utility.hpp>

#include <mesh.pp/fileutility.hpp>
#include <mesh.pp/cryptoutility.hpp>

using namespace BlockchainMessage;
namespace filesystem = boost::filesystem;

using std::string;

namespace publiqpp
{
namespace detail
{
class blockchain_internals
{
public:
    blockchain_internals(filesystem::path const& path)
        : m_header("header", path, 1000, 1, detail::get_putl())
        , m_blockchain("block", path, 10000, 1, detail::get_putl())
    {
    }

    string m_last_hash;
    BlockHeader m_last_header;
    meshpp::vector_loader<BlockHeader> m_header;
    meshpp::vector_loader<SignedBlock> m_blockchain;
};
}

blockchain::blockchain(boost::filesystem::path const& fs_blockchain)
    : m_pimpl(new detail::blockchain_internals(fs_blockchain))
{
    if (length() > 0)
        update_state();
}

blockchain::~blockchain() = default;

void blockchain::save()
{
    m_pimpl->m_header.save();
    m_pimpl->m_blockchain.save();
}

void blockchain::commit() noexcept
{
    m_pimpl->m_header.commit();
    m_pimpl->m_blockchain.commit();
}

void blockchain::discard() noexcept
{
    m_pimpl->m_header.discard();
    m_pimpl->m_blockchain.discard();

    if (length() > 0)
        update_state();
}

void blockchain::clear()
{
    m_pimpl->m_header.clear();
    m_pimpl->m_blockchain.clear();
}

void blockchain::update_state()
{
    assert(0 != length());
    if (0 == length())
        return;

    SignedBlock const& signed_block = at(length() - 1);

    m_pimpl->m_last_header = signed_block.block_details.header;
    m_pimpl->m_last_hash = meshpp::hash(signed_block.block_details.to_string());
}

uint64_t blockchain::length() const
{
    return m_pimpl->m_blockchain.size();
}

string blockchain::last_hash() const
{
    return m_pimpl->m_last_hash;
}

BlockHeader const& blockchain::last_header() const
{
    return m_pimpl->m_last_header;
}

BlockHeaderExtended blockchain::last_header_ex() const
{
    BlockHeaderExtended result;

    result.block_number = last_header().block_number;
    result.c_const = last_header().c_const;
    result.c_sum = last_header().c_sum;
    result.delta = last_header().delta;
    result.prev_hash = last_header().prev_hash;
    result.time_signed = last_header().time_signed;
    result.block_hash = last_hash();

    return result;
}

void blockchain::insert(SignedBlock const& signed_block)
{
    Block const& block = signed_block.block_details;

    uint64_t block_number = block.header.block_number;

    if (block_number != length())
        throw std::runtime_error("Wrong block to insert!");

    m_pimpl->m_header.push_back(block.header);
    m_pimpl->m_blockchain.push_back(signed_block);

    update_state();
}

BlockchainMessage::SignedBlock const& blockchain::at(uint64_t number) const
{
    return m_pimpl->m_blockchain.as_const().at(number);
}

BlockHeader const& blockchain::header_at(uint64_t number) const
{
    return m_pimpl->m_header.as_const().at(number);
}
BlockHeaderExtended blockchain::header_ex_at(uint64_t number) const
{
    BlockHeaderExtended result;
    if (number != m_pimpl->m_blockchain.size() - 1)
    {
        auto const& header = header_at(number);

        result.block_number = header.block_number;
        result.c_const = header.c_const;
        result.c_sum = header.c_sum;
        result.delta = header.delta;
        result.prev_hash = header.prev_hash;
        result.time_signed = header.time_signed;
        result.block_hash = m_pimpl->m_header.as_const().at(number + 1).prev_hash;
    }
    else
        result = last_header_ex();

    return result;
}

void blockchain::remove_last_block()
{
    if (length() == 1)
        throw std::runtime_error("Nothing to remove!");

    m_pimpl->m_header.pop_back();
    m_pimpl->m_blockchain.pop_back();

    update_state();
}

string blockchain::get_miner(SignedBlock const& signed_block)
{
    string miner;
    for (auto const& reward_item : signed_block.block_details.rewards)
    {
        if (reward_item.reward_type == RewardType::miner)
        {
            if (false == miner.empty())
                throw std::logic_error("blockchain::get_miner: false == miner.empty()");
            
            miner = reward_item.to;
        }
    }

    if (miner.empty())
        miner = signed_block.authorization.address;

    return miner;
}
}
