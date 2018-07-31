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

    const uint64_t mine_threshold = 100000000;

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

    uint64_t dist(string const& key, string const& hash) const
    {
        // quick solution, may be not the best
        string key_hash = meshpp::hash(key);

        return meshpp::distance(key_hash, hash);
    }

    bool mine_allowed() const
    {
        // check time after previous block

        system_clock::time_point current_time_point = system_clock::now();
        system_clock::time_point previous_time_point = system_clock::from_time_t(m_header.sign_time.tm);

        //  both previous_time_point and current_time_point keep track of UTC time

        chrono::minutes diff_minutes = chrono::duration_cast<chrono::minutes>(current_time_point - previous_time_point);
        auto num_minutes = diff_minutes.count();
        chrono::hours diff_hours = chrono::duration_cast<chrono::hours>(current_time_point - previous_time_point);
        auto num_hours = diff_hours.count();

        return num_minutes >= BLOCK_MINE_DELAY || num_hours > 0;
    }

    bool apply_allowed() const
    {
        // check time after previous mine

        system_clock::time_point current_time_point = system_clock::now();
        system_clock::time_point previous_time_point = system_clock::from_time_t(tmp_header.sign_time.tm);

        //  both previous_time_point and current_time_point keep track of UTC time

        chrono::seconds diff_seconds = chrono::duration_cast<chrono::seconds>(current_time_point - previous_time_point);
        auto num_seconds = diff_seconds.count();
        chrono::minutes diff_minutes = chrono::duration_cast<chrono::minutes>(current_time_point - previous_time_point);
        auto num_minutes = diff_minutes.count();
        chrono::hours diff_hours = chrono::duration_cast<chrono::hours>(current_time_point - previous_time_point);
        auto num_hours = diff_hours.count();

        return num_seconds >= BLOCK_APPLY_DELAY || num_minutes > 0 || num_hours > 0;
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

uint64_t blockchain::calc_delta(string const& key, uint64_t amount, 
                                BlockchainMessage::Block const& block) const
{
    uint64_t d = m_pimpl->dist(key, block.block_header.previous_hash);
    uint64_t delta = amount / (d * block.block_header.consensus_const);
    
    if (delta > DELTA_MAX)
        delta = DELTA_MAX;

    return delta;
}

bool blockchain::mine_block(meshpp::private_key const& pv_key, uint64_t amount,
                            publiqpp::transaction_pool const& transaction_pool)
{
    // miner node must have minimal amount in balance
    if (amount < m_pimpl->mine_threshold)
        return false;

    // between two blocks BLOCK_MINE_DELAY must pass
    if (false == m_pimpl->mine_allowed())
        return false;

    uint64_t block_number = length() - 1;

    SignedBlock prev_signed_block;
    at(block_number, prev_signed_block);

    beltpp::packet package_prev_block = std::move(prev_signed_block.block_details);
    string prev_block_hash = meshpp::hash(package_prev_block.to_string());

    Block prev_block;
    std::move(package_prev_block).get(prev_block);

    string key = pv_key.get_public_key().to_string();
    uint64_t delta = calc_delta(key, amount, prev_block);

    // fill new block header data
    ++block_number;
    BlockHeader block_header;
    block_header.block_number = block_number;
    block_header.consensus_delta = delta;
    block_header.consensus_const = prev_block.block_header.consensus_const;
    block_header.consensus_sum = prev_block.block_header.consensus_sum + delta;
    block_header.previous_hash = prev_block_hash;
    block_header.sign_time.tm = system_clock::to_time_t(system_clock::now());

    // update consensus_const if needed
    if (delta > DELTA_UP)
    {
        size_t step = 0;
        BlockHeader prev_header;
        header_at(block_number, prev_header);

        while (prev_header.consensus_delta > DELTA_UP && 
               step < DELTA_STEP && prev_header.block_number > 0)
        {
            ++step;
            header_at(prev_header.block_number - 1, prev_header);
        }

        if (step >= DELTA_STEP)
            block_header.consensus_const = prev_block.block_header.consensus_const * 2;
    }
    else
    if (delta < DELTA_DOWN && block_header.consensus_const > 1)
    {
        size_t step = 0;
        BlockHeader prev_header;
        header_at(block_number, prev_header);

        while (prev_header.consensus_delta < DELTA_DOWN && 
               step < DELTA_STEP && prev_header.block_number > 0)
        {
            ++step;
            header_at(prev_header.block_number - 1, prev_header);
        }

        if (step >= DELTA_STEP)
            block_header.consensus_const = prev_block.block_header.consensus_const / 2;
    }

    Block block;
    block.block_header = block_header; // move is not allowed

    //TODO manage rewards

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
    meshpp::signature sgn = pv_key.sign(block.to_string());

    SignedBlock signed_block;
    signed_block.signature = sgn.base64;
    signed_block.authority = sgn.pb_key.to_string();
    signed_block.block_details = std::move(block);
    
    m_pimpl->step_enabled = true;
    m_pimpl->tmp_block = std::move(signed_block);
    m_pimpl->tmp_header = std::move(block_header);

    return true;
}

bool blockchain::tmp_header(BlockchainMessage::BlockHeader& block_header) const
{
    if (length() >= m_pimpl->tmp_header.block_number)
        return false;

    block_header = m_pimpl->tmp_header;

    return true;
}

bool blockchain::tmp_block(BlockchainMessage::SignedBlock& signed_block) const
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
