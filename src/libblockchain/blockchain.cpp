#include "blockchain.hpp"

#include "common.hpp"
#include "message.hpp"

#include <belt.pp/utility.hpp>

#include <mesh.pp/fileutility.hpp>

using namespace BlockchainMessage;
namespace filesystem = boost::filesystem;

namespace publiqpp
{
namespace detail
{
class blockchain_internals
{
public:
    blockchain_internals(filesystem::path const& path)
        : m_header("header", path, 1000, detail::get_putl())
        , m_blockchain("block", path, 10000, detail::get_putl())
    {
    }

    BlockHeader m_last_header;
    meshpp::vector_loader<BlockHeader> m_header;
    meshpp::vector_loader<SignedBlock> m_blockchain;
};
}

blockchain::blockchain(boost::filesystem::path const& fs_blockchain)
    : m_pimpl(new detail::blockchain_internals(fs_blockchain))
{
    update_header();
}

blockchain::~blockchain()
{

}

void blockchain::save()
{
    m_pimpl->m_header.save();
    m_pimpl->m_blockchain.save();
}

void blockchain::commit()
{
    m_pimpl->m_header.commit();
    m_pimpl->m_blockchain.commit();
}

void blockchain::discard()
{
    m_pimpl->m_header.discard();
    m_pimpl->m_blockchain.discard();

    update_header();
}

void blockchain::update_header()
{
    if (length() > 0)
        header_at(length() - 1, m_pimpl->m_last_header);
}

uint64_t blockchain::length() const
{
    return m_pimpl->m_blockchain.as_const().size();
}

void blockchain::header(BlockHeader& block_header) const
{
    block_header = m_pimpl->m_last_header;
}

void blockchain::insert(SignedBlock const& signed_block)
{
    Block block;
    signed_block.block_details.get(block);

    uint64_t block_number = block.header.block_number;

    if (block_number != length())
        throw std::runtime_error("Wrong block to insert!");

    m_pimpl->m_last_header = block.header;
    m_pimpl->m_header.push_back(block.header);
    m_pimpl->m_blockchain.push_back(signed_block);
}

void blockchain::at(uint64_t number, SignedBlock& signed_block) const
{
    if (number >= length())
        throw std::runtime_error("There is no block with number:" + std::to_string(number));

    signed_block = m_pimpl->m_blockchain.as_const().at(number);
}

void blockchain::header_at(uint64_t number, BlockHeader& block_header) const
{
    if (number >= length())
        throw std::runtime_error("There is no header with number:" + std::to_string(number));

    block_header = m_pimpl->m_header.as_const().at(number);
}

void blockchain::remove_last_block()
{
    if (length() == 1)
        throw std::runtime_error("Nothing to remove!");

    m_pimpl->m_header.pop_back();
    m_pimpl->m_blockchain.pop_back();

    update_header();
}
}
