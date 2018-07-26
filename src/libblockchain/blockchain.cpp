#include "blockchain.hpp"

#include "data.hpp"
#include "message.hpp"

#include <mesh.pp/fileutility.hpp>
#include <mesh.pp/cryptoutility.hpp>

using namespace BlockchainMessage;
namespace filesystem = boost::filesystem;
using std::string;
using std::vector;

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
    const uint64_t mine_amount = 100000000;

    bool step_enabled;
    SignedBlock tmp_block;
    BlockHeader tmp_header;

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

    uint64_t dist(string key, string hash)
    {
        //TODO
        return 10000;
    }

    bool mine_allowed()
    {
        //TODO check time after previous block
        return true;
    }

    bool apply_allowed()
    {
        //TODO check time after previous mine
        return true;
    }
};
}

blockchain::blockchain(boost::filesystem::path const& fs_blockchain)
    : m_pimpl(new detail::blockchain_internals(fs_blockchain))
{
    update_header();
    m_pimpl->step_enabled = false;
}

blockchain::~blockchain()
{

}

void blockchain::update_header()
{
    if (length() > 0)
    {
        SignedBlock signed_block;
        at(length() - 1, signed_block);

        Block block;
        std::move(signed_block.block_details).get(block);
        m_pimpl->m_header = std::move(block.block_header);
    }
}

uint64_t blockchain::length() const
{
    return m_pimpl->m_length.as_const()->value;
}

void blockchain::header(BlockchainMessage::BlockHeader& block_header) const
{
    if(length() > 0)
        block_header = m_pimpl->m_header;
    else
    {
        block_header.block_number = 0;
        block_header.consensus_sum = 0;
        block_header.consensus_delta = 0;
        block_header.consensus_const = 1;
        block_header.previous_hash = "Ice Age";
    }
}

bool blockchain::insert(beltpp::packet const& packet)
{
    if (packet.type() != SignedBlock::rtt)
        return false;

    SignedBlock signed_block;
    packet.get(signed_block);

    Block block;
    signed_block.block_details.get(block);

    uint64_t block_number = block.block_header.block_number;

    if (block_number != length())
        return false;

    string hash_id = m_pimpl->get_df_id(block_number);
    string file_name("df" + hash_id + ".bchain");

    blockchain_data_loader file_data(m_pimpl->m_path / file_name);

    m_pimpl->m_header = block.block_header;
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

    string hash_id = m_pimpl->get_df_id(number);
    string file_name("df" + hash_id + ".bchain");

    blockchain_data_loader file_data(m_pimpl->m_path / file_name);

    SignedBlock signed_block;
    file_data->blocks[number].get(signed_block);

    Block block;
    std::move(signed_block.block_details).get(block);

    block_header = std::move(block.block_header);

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

uint64_t blockchain::calc_delta(string key, uint64_t amount, BlockchainMessage::Block& block)
{
    uint64_t d = m_pimpl->dist(key, block.block_header.previous_hash);
    uint64_t delta = amount / (d * block.block_header.consensus_const);
    
    if (delta > m_pimpl->delta_max)
        delta = m_pimpl->delta_max;

    return delta;
}

bool blockchain::mine_block(string key, 
                            uint64_t amount,
                            publiqpp::transaction_pool& transaction_pool)
{
    if (amount < m_pimpl->mine_amount)
        return false;

    if (false == m_pimpl->mine_allowed())
        return false;

    uint64_t block_number = length();

    SignedBlock prev_signed_block;
    at(block_number, prev_signed_block);

    beltpp::packet package_prev_block = std::move(prev_signed_block.block_details);
    vector<char> packet_vec = package_prev_block.save();
    string prev_block_hash = meshpp::hash(packet_vec.begin(), packet_vec.end());

    Block prev_block;
    package_prev_block.get(prev_block);

    uint64_t delta = calc_delta(key, amount, prev_block);

    ++block_number;
    BlockHeader block_header;
    block_header.block_number = block_number;
    block_header.consensus_delta = delta;
    block_header.consensus_const = prev_block.block_header.consensus_const;
    block_header.consensus_sum = prev_block.block_header.consensus_sum + delta;
    block_header.previous_hash = prev_block_hash;

    if (delta > m_pimpl->delta_up)
    {
        size_t step = 1;
        uint64_t _delta = delta;

        while (_delta > m_pimpl->delta_up && step < m_pimpl->delta_step && block_number > 0)
        {
            SignedBlock _prev_signed_block;
            at(block_number, _prev_signed_block);

            Block _prev_block;
            std::move(_prev_signed_block.block_details).get(_prev_block);

            --block_number;
            ++step;
            _delta = _prev_block.block_header.consensus_delta;
        }

        if (step >= m_pimpl->delta_step)
            block_header.consensus_const = prev_block.block_header.consensus_const * 2;
    }
    else
    if (delta < m_pimpl->delta_down && block_header.consensus_const > 1)
    {
        size_t step = 1;
        uint64_t _delta = delta;

        while (_delta < m_pimpl->delta_down && step < m_pimpl->delta_step && block_number > 0)
        {
            SignedBlock _prev_signed_block;
            at(block_number, _prev_signed_block);

            Block _prev_block;
            std::move(_prev_signed_block.block_details).get(_prev_block);

            --block_number;
            ++step;
            _delta = _prev_block.block_header.consensus_delta;
        }

        if (step >= m_pimpl->delta_step)
            block_header.consensus_const = prev_block.block_header.consensus_const / 2;
    }

    Block block;
    block.block_header = block_header;
    // copy transactions from pool to block
    std::vector<std::string> keys;
    transaction_pool.get_keys(keys);

    //TODO here we should have keys ordered by transaction time
    for (auto& it : keys)
    {
        B_UNUSED(it);
        //TODO move transactions to the block
    }

    // save block as tmp
    SignedBlock signed_block;
    signed_block.authority = key;
    signed_block.signature = "signature"; //TODO
    signed_block.block_details = std::move(block);

    m_pimpl->step_enabled = true;
    m_pimpl->tmp_block = std::move(signed_block);
    m_pimpl->tmp_header = std::move(block_header);

    return true;
}

bool blockchain::tmp_header(BlockchainMessage::BlockHeader& block_header)
{
    if (length() >= m_pimpl->tmp_header.block_number)
        return false;

    block_header = m_pimpl->tmp_header;

    return true;
}

bool blockchain::tmp_block(BlockchainMessage::SignedBlock& signed_block)
{
    if (length() >= m_pimpl->tmp_header.block_number)
        return false;

    signed_block = m_pimpl->tmp_block;

    return true;
}

void blockchain::step_apply()
{
    if (false == m_pimpl->step_enabled)
        return;

    if (false == m_pimpl->apply_allowed())
        return;

    SignedBlock signed_block;

    if (tmp_block(signed_block))
        insert(std::move(signed_block));

    step_disable();
}

void blockchain::step_disable()
{
    m_pimpl->step_enabled = false;
}

}
