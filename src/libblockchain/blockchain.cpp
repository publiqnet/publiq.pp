#include "blockchain.hpp"

#include "data.hpp"
#include "message.hpp"

#include <belt.pp/utility.hpp>

#include <mesh.pp/fileutility.hpp>
#include <mesh.pp/cryptoutility.hpp>

#include <chrono>

using namespace BlockchainMessage;
namespace filesystem = boost::filesystem;

using std::string;
using std::vector;

namespace chrono = std::chrono;
using chrono::system_clock;

using number_loader = meshpp::file_loader<Data::Number,
                                          &Data::Number::from_string,
                                          &Data::Number::to_string>;
using number_locked_loader = meshpp::file_locker<number_loader>;

using blockchain_data_loader = meshpp::file_loader<BlockchainFileData,
                                                   &BlockchainFileData::from_string,
                                                   &BlockchainFileData::to_string>;

namespace publiqpp
{
namespace detail
{
class blockchain_internals
{
public:
    blockchain_internals(filesystem::path const& path)
        : m_path(path)
        , m_length(path / "length.txt")
    {

    }

    filesystem::path m_path;
    number_locked_loader m_length;
    BlockHeader m_header;

    string get_df_id(uint64_t number) const
    {
        int i = 10000 + number % 4096 + 1;

        string s = std::to_string(i);

        return s.substr(1, 4);

        //return "0000"; //Debug mode
    }
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

void blockchain::update_header()
{
    if (length() > 0)
        header_at(length() - 1, m_pimpl->m_header);
}

uint64_t blockchain::length() const
{
    return m_pimpl->m_length.as_const()->value;
}

void blockchain::header(BlockHeader& block_header) const
{
    block_header = m_pimpl->m_header;
}

bool blockchain::insert(beltpp::packet const& packet)
{
    if (packet.type() != SignedBlock::rtt)
        return false;

    SignedBlock signed_block;
    packet.get(signed_block);

    Block block;
    signed_block.block_details.get(block);

    uint64_t block_number = block.header.block_number;

    if (block_number != length())
        return false;

    string hash_id = m_pimpl->get_df_id(block_number);
    string file_name("df" + hash_id + ".bchain");

    blockchain_data_loader file_data(m_pimpl->m_path / file_name);

    m_pimpl->m_header = block.header;
    m_pimpl->m_length->value = block_number + 1;
    file_data->blocks[block_number] = signed_block;

    file_data.save();
    m_pimpl->m_length.save();

    return true;
}

bool blockchain::at(uint64_t number, SignedBlock& signed_block) const
{
    if (number >= length())
        return false;

    string hash_id = m_pimpl->get_df_id(number);
    string file_name("df" + hash_id + ".bchain");

    blockchain_data_loader file_data(m_pimpl->m_path / file_name);

    file_data->blocks[number].get(signed_block);

    return true;
}

bool blockchain::header_at(uint64_t number, BlockHeader& block_header) const
{
    if (number >= length())
        return false;

    SignedBlock signed_block;
    at(number, signed_block);

    Block block;
    std::move(signed_block.block_details).get(block);

    block_header = std::move(block.header);

    return true;
}

void blockchain::remove_last_block()
{
    uint64_t number = length() - 1;

    if (number == 0)
        throw std::runtime_error("Nothing to remove!");

    string hash_id = m_pimpl->get_df_id(number);
    string file_name("df" + hash_id + ".bchain");

    blockchain_data_loader file_data(m_pimpl->m_path / file_name);

    file_data->blocks.erase(number);
    m_pimpl->m_length->value = number;

    file_data.save();
    m_pimpl->m_length.save();

    update_header();
}

}
