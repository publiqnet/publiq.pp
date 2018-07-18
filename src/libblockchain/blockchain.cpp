#include "blockchain.hpp"

#include "data.hpp"
#include "message.hpp"

#include <mesh.pp/fileutility.hpp>

using namespace BlockchainMessage;
namespace filesystem = boost::filesystem;
using std::string;

using number_loader = meshpp::file_loader<Data::Number, &Data::Number::string_loader, &Data::Number::string_saver>;
using number_locked_loader = meshpp::file_locker<number_loader>;

using blockchain_data_loader = meshpp::file_loader<BlockchainFileData,
                                                   &BlockchainFileData::string_loader,
                                                   &BlockchainFileData::string_saver>;

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

}
blockchain::~blockchain()
{

}

size_t blockchain::length() const
{
    return m_pimpl->m_length.as_const()->value;
}

void blockchain::insert(beltpp::packet const& packet)
{
    if (packet.type() != SignedBlock::rtt)
        throw std::runtime_error("Unknown object typeid to insert: " + std::to_string(packet.type()));

    SignedBlock signed_block;
    packet.get(signed_block);

    Block block;
    signed_block.block_details.get(block);

    uint64_t number = block.number;

    if ( number != length() + 1)
        throw std::runtime_error("Wrong block is goinf to insert! number:" + std::to_string(number));

    string hash_id = m_pimpl->get_df_id(number);
    string file_name("df" + hash_id + ".bchain");

    blockchain_data_loader file_data(m_pimpl->m_path / file_name);

    file_data->blocks[number] = signed_block;
    m_pimpl->m_length->value = number;

    file_data.save();
    m_pimpl->m_length.save();
}

bool blockchain::at(uint64_t number, beltpp::packet& signed_block) const
{
    if (number > length())
        return false;

    string hash_id = m_pimpl->get_df_id(number);
    string file_name("df" + hash_id + ".bchain");

    blockchain_data_loader file_data(m_pimpl->m_path / file_name);

    ::detail::assign_packet(signed_block, file_data->blocks[number]);

    return true;
}

void blockchain::remove_last_block()
{
    uint64_t number = length();

    if (number == 0)
        throw std::runtime_error("Unable remove genesis block");

    string hash_id = m_pimpl->get_df_id(number);
    string file_name("df" + hash_id + ".bchain");

    blockchain_data_loader file_data(m_pimpl->m_path / file_name);

    file_data->blocks.erase(number);
    m_pimpl->m_length->value = number - 1;

    file_data.save();
    m_pimpl->m_length.save();
}

}
