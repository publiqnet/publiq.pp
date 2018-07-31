#include "transaction_pool.hpp"

#include "data.hpp"
#include "message.hpp"

#include <mesh.pp/fileutility.hpp>
#include <mesh.pp/cryptoutility.hpp>

using namespace BlockchainMessage;
namespace filesystem = boost::filesystem;

using std::string;

using hash_index_loader = meshpp::file_loader<Data::StringSet, 
                                              &Data::StringSet::from_string,
                                              &Data::StringSet::to_string>;
using hash_index_locked_loader = meshpp::file_locker<hash_index_loader>;

using transaction_data_loader = meshpp::file_loader<TransactionFileData,
                                                    &TransactionFileData::from_string,
                                                    &TransactionFileData::to_string>;

namespace publiqpp
{

namespace detail
{
class transaction_pool_internals
{
public:
    transaction_pool_internals(filesystem::path const& path)
        : m_path(path)
        , m_index(path / "transactions.tpool.index")
    {
        // Load transactions
        std::unordered_set<string> filename_set;
        for (string const& packet_hash : m_index->dictionary)
            filename_set.insert(string("df" + get_df_id(packet_hash) + ".tpool"));

        for (string const& file_name : filename_set)
        {
            transaction_data_loader file_data(m_path / file_name);

            for (auto& it : file_data.as_const()->actions)
            {
                SignedTransaction signed_transaction;
                std::move(it.second).get(signed_transaction);

                m_transactions[it.first] = signed_transaction;
            }
        }
    }

    filesystem::path m_path;
    hash_index_locked_loader m_index;
    std::unordered_map<string, SignedTransaction> m_transactions;

    string get_df_id(string const& hash) const
    {
        string _hash = meshpp::base64_to_hex(hash);
        string s = _hash.substr(_hash.size()-3, 3);
        int i = 10000 + std::stoi(s, 0, 16) + 1;
        
        s = std::to_string(i);
        
        return s.substr(1, 4);

        //return "0000"; //Debug mode
    }
};
}

transaction_pool::transaction_pool(filesystem::path const& fs_transaction_pool)
    : m_pimpl(new detail::transaction_pool_internals(fs_transaction_pool))
{

}
transaction_pool::~transaction_pool()
{

}

bool transaction_pool::insert(beltpp::packet const& package)
{
    if (package.type() != SignedTransaction::rtt)
        return false;

    string packet_hash = meshpp::hash(package.to_string());
    string hash_id = m_pimpl->get_df_id(packet_hash);
    string file_name("df" + hash_id + ".tpool");

    m_pimpl->m_index->dictionary.insert(packet_hash);
    transaction_data_loader file_data(m_pimpl->m_path / file_name);

    SignedTransaction signed_transaction;
    package.get(signed_transaction);

    file_data->actions[packet_hash] = signed_transaction;
    m_pimpl->m_transactions[packet_hash] = signed_transaction;
    
    file_data.save();
    m_pimpl->m_index.save();

    return true;
}

bool transaction_pool::at(string const& key, SignedTransaction& signed_transaction) const
{
    if (contains(key))
        return false;

    signed_transaction = m_pimpl->m_transactions[key];
    
    return true;
}

bool transaction_pool::remove(string const& key)
{
    if (contains(key))
        return false;

    string hash_id = m_pimpl->get_df_id(key);
    string file_name("df" + hash_id + ".tpool");
    transaction_data_loader file_data(m_pimpl->m_path / file_name);
    
    file_data->actions.erase(key);
    m_pimpl->m_transactions.erase(key);
    m_pimpl->m_index->dictionary.erase(key);

    file_data.save();
    m_pimpl->m_index.save();

    return true;
}

size_t transaction_pool::length() const
{
    return m_pimpl->m_transactions.size();
}

bool transaction_pool::contains(string const& key) const
{
    return m_pimpl->m_index->dictionary.find(key) != m_pimpl->m_index->dictionary.end();
}

void transaction_pool::get_amounts(std::string const& key, std::vector<std::pair<std::string, uint64_t>>& amounts, bool in_out) const
{
    for (auto &it : m_pimpl->m_transactions)
    {
        Transfer transfer;
        it.second.transaction_details.action.get(transfer);

        if (in_out && transfer.to == key)
            amounts.push_back(std::pair<std::string, uint64_t>(it.first, transfer.amount));
        else if (!in_out && transfer.from == key)
            amounts.push_back(std::pair<std::string, uint64_t>(it.first, transfer.amount + it.second.transaction_details.fee));
    }
}

void transaction_pool::get_keys(std::vector<std::string> &keys) const
{
    keys.clear();

    for (auto &it : m_pimpl->m_transactions)
        keys.push_back(it.first);
}

}
