#include "transaction_pool.hpp"
#include "node_internals.hpp"
#include "transaction_handler.hpp"
#include "message.tmpl.hpp"

#include <mesh.pp/fileutility.hpp>
#include <mesh.pp/cryptoutility.hpp>

#include <unordered_map>

using namespace BlockchainMessage;
namespace filesystem = boost::filesystem;

using std::string;
using std::vector;
using std::unordered_map;

namespace publiqpp
{

namespace detail
{
class transaction_pool_internals
{
public:
    transaction_pool_internals(filesystem::path const& path)
        : m_transactions("transactions", path, 100, 10, detail::get_putl())
    {
    }

    meshpp::vector_loader<SignedTransaction> m_transactions;
};
}

transaction_pool::transaction_pool(filesystem::path const& fs_transaction_pool)
    : m_pimpl(new detail::transaction_pool_internals(fs_transaction_pool))
{

}

transaction_pool::~transaction_pool() = default;

void transaction_pool::save()
{
    m_pimpl->m_transactions.save();
}

void transaction_pool::commit() noexcept
{
    m_pimpl->m_transactions.commit();
}

void transaction_pool::discard() noexcept
{
    m_pimpl->m_transactions.discard();
}

void transaction_pool::clear()
{
    m_pimpl->m_transactions.clear();
}

void transaction_pool::push_back(SignedTransaction const& signed_transaction)
{
    m_pimpl->m_transactions.push_back(signed_transaction);
}

void transaction_pool::pop_back()
{
    m_pimpl->m_transactions.pop_back();
}

BlockchainMessage::SignedTransaction const& transaction_pool::at(size_t index) const
{
    return m_pimpl->m_transactions.as_const().at(index);
}
BlockchainMessage::SignedTransaction& transaction_pool::ref_at(size_t index) const
{
    return m_pimpl->m_transactions.at(index);
}

size_t transaction_pool::length() const
{
    return m_pimpl->m_transactions.size();
}

void load_transaction_cache(publiqpp::detail::node_internals& impl,
                            bool only_pool)
{
    if (false == only_pool)
    {
        impl.writeln_node("Loading recent blocks to cache");

        std::chrono::system_clock::time_point time_signed_head;

        uint64_t block_count = impl.m_blockchain.length();
        for (uint64_t block_index = block_count - 1;
             block_index < block_count;
             --block_index)
        {
            SignedBlock const& signed_block = impl.m_blockchain.at(block_index);

            Block const& block = signed_block.block_details;

            std::chrono::system_clock::time_point time_signed =
                    std::chrono::system_clock::from_time_t(block.header.time_signed.tm);
            if (block_index == block_count - 1)
                time_signed_head = time_signed;
            else if (time_signed_head - time_signed >
                     std::chrono::hours(TRANSACTION_MAX_LIFETIME_HOURS) +
                     std::chrono::seconds(NODES_TIME_SHIFT))
                break; //   because all transactions in this block must be expired

            for (auto& item : block.signed_transactions)
            {
                if (false == impl.m_transaction_cache.add_chain(item))
                    throw std::logic_error("inconsistent stored blockchain");
            }
        }
    }

    for (size_t index = 0; index != impl.m_transaction_pool.length(); ++index)
    {
        auto const& item = impl.m_transaction_pool.at(index);
        bool complete = action_is_complete(impl, item);

        if (false == impl.m_transaction_cache.add_pool(item, complete))
            throw std::logic_error("inconsistent stored pool");
    }
}

}
