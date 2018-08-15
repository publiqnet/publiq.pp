#include "transaction_pool.hpp"
#include "common.hpp"

#include "message.hpp"

#include <mesh.pp/fileutility.hpp>
#include <mesh.pp/cryptoutility.hpp>

using namespace BlockchainMessage;
namespace filesystem = boost::filesystem;

using std::string;
using std::vector;

namespace publiqpp
{

namespace detail
{
class transaction_pool_internals
{
public:
    transaction_pool_internals(filesystem::path const& path)
        :m_transactions("transactions", path, detail::get_putl())
    {
    }

    std::vector<Reward> tmp_rewards; // for test only
    meshpp::map_loader<SignedTransaction> m_transactions;
};
}

transaction_pool::transaction_pool(filesystem::path const& fs_transaction_pool)
    : m_pimpl(new detail::transaction_pool_internals(fs_transaction_pool))
{

}

transaction_pool::~transaction_pool()
{

}

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

void transaction_pool::insert(SignedTransaction const& signed_transaction)
{
    string key = meshpp::hash(signed_transaction.to_string());

    if (!contains(key))
        m_pimpl->m_transactions.insert(key, signed_transaction);
}

void transaction_pool::insert_reward(Reward const& reward)
{
    m_pimpl->tmp_rewards.push_back(reward);
}

void transaction_pool::at(string const& key, SignedTransaction& signed_transaction) const
{
    if (!contains(key))
        throw std::runtime_error("There is no such a transaction!");

    signed_transaction = m_pimpl->m_transactions.as_const().at(key);
}

void transaction_pool::remove(string const& key)
{
    if (m_pimpl->m_transactions.contains(key))
        m_pimpl->m_transactions.erase(key);
}

size_t transaction_pool::length() const
{
    return m_pimpl->m_transactions.as_const().keys().size();
}

void transaction_pool::get_keys(std::vector<std::string> &keys) const
{
    keys.clear();

    for (auto& key : m_pimpl->m_transactions.as_const().keys())
        keys.push_back(key);
}

bool transaction_pool::contains(std::string const& key) const
{
    return m_pimpl->m_transactions.as_const().contains(key);
}

void transaction_pool::grant_rewards(vector<Reward>& rewards) const
{
    rewards.clear();

    for (auto& reward : m_pimpl->tmp_rewards)
        rewards.push_back(reward);

    m_pimpl->tmp_rewards.clear();

    //TODO add something meaningfull
}
}
