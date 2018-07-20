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

    const size_t delta_step = 10;
    const uint64_t delta_max = 120000000;
    const uint64_t delta_up = 100000000;
    const uint64_t delta_down = 80000000;

    filesystem::path m_path;
    number_locked_loader m_length;

    string get_df_id(uint64_t number) const
    {
        int i = 10000 + number % 4096 + 1;

        string s = std::to_string(i);

        return s.substr(1, 4);

        //return "0000"; //Debug mode
    }

    uint64_t dist(string key, string hash)
    {
        //TODO
        return 10000;
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

uint64_t blockchain::get_delta(string key, BlockchainMessage::Block& block, uint64_t amount)
{
    uint64_t d = m_pimpl->dist(key, block.previous_hash);
    uint64_t delta = amount / (d * block.consensus_const);
    
    if (delta > m_pimpl->delta_max)
        delta = m_pimpl->delta_max;

    return delta;
}

void blockchain::mine_block(string key, 
                            uint64_t amount,
                            publiqpp::transaction_pool& transaction_pool, 
                            SignedBlock& signed_block)
{
    uint64_t number = length();

    beltpp::packet prev_packet;
    at(number, prev_packet);

    SignedBlock prev_signed_block;
    std::move(prev_packet).get(prev_signed_block);

    Block prev_block;
    std::move(prev_signed_block.block_details).get(prev_block);

    uint64_t delta = get_delta(key, prev_block, amount);

    Block block;
    block.number = number + 1;
    block.consensus_delta = delta;
    block.consensus_const = prev_block.consensus_const;
    block.consensus_summary = prev_block.consensus_summary + delta;
    block.previous_hash = "previous_hash"; //TODO

    if (delta > m_pimpl->delta_up)
    {
        size_t step = 1;
        uint64_t _delta = delta;

        while (_delta > m_pimpl->delta_up && step < m_pimpl->delta_step && number > 0)
        {
            beltpp::packet _prev_packet;
            at(number, _prev_packet);

            SignedBlock _prev_signed_block;
            std::move(_prev_packet).get(_prev_signed_block);

            Block _prev_block;
            std::move(_prev_signed_block.block_details).get(_prev_block);

            --number;
            ++step;
            _delta = _prev_block.consensus_delta;
        }

        if (step >= m_pimpl->delta_step)
            block.consensus_const = prev_block.consensus_const * 2;
    }
    else
    if (delta < m_pimpl->delta_down && block.consensus_const > 1)
    {
        size_t step = 1;
        uint64_t _delta = delta;

        while (_delta < m_pimpl->delta_down && step < m_pimpl->delta_step && number > 0)
        {
            beltpp::packet _prev_packet;
            at(number, _prev_packet);

            SignedBlock _prev_signed_block;
            std::move(_prev_packet).get(_prev_signed_block);

            Block _prev_block;
            std::move(_prev_signed_block.block_details).get(_prev_block);

            --number;
            ++step;
            _delta = _prev_block.consensus_delta;
        }

        if (step >= m_pimpl->delta_step)
            block.consensus_const = prev_block.consensus_const / 2;
    }

    // copy transactions from pool to block
    std::vector<std::string> keys;
    transaction_pool.get_keys(keys);

    //TODO here we should have keys ordered by transaction time
    for (auto& it : keys)
    {
        //TODO move transactions to the block
    }

    // sign block to return
    signed_block.authority = key;
    signed_block.signature = "signature"; //TODO
    signed_block.block_details = std::move(block);
}

}
