#include "transaction_pool.hpp"

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

void transaction_pool::commit()
{
    m_pimpl->m_transactions.commit();
}

void transaction_pool::discard()
{
    m_pimpl->m_transactions.discard();
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

size_t transaction_pool::length() const
{
    return m_pimpl->m_transactions.size();
}

}
