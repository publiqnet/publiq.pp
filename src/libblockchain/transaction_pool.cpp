#include "transaction_pool.hpp"

#include "data.hpp"
#include "message.hpp"

#include <mesh.pp/cryptoutility.hpp>
#include <mesh.pp/fileutility.hpp>

#include <string>
#include <algorithm>

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
        : m_path(path)
        , m_index(path / "transactions.tpool.index")
    {

    }
    using hash_index_loader = meshpp::file_loader<Data::HashIndex, &Data::HashIndex::string_loader, &Data::HashIndex::string_saver>;
    using hash_index_locked_loader = meshpp::file_locker<hash_index_loader>;

    filesystem::path m_path;
    hash_index_locked_loader m_index;
};
}

transaction_pool::transaction_pool(boost::filesystem::path const& fs_transaction_pool)
    : m_pimpl(new detail::transaction_pool_internals(fs_transaction_pool))
{

}
transaction_pool::~transaction_pool()
{

}

vector<std::string> transaction_pool::keys() const
{
    return m_pimpl->m_index.as_const()->keys;
}

void transaction_pool::insert(beltpp::packet const& packet)
{
    if (packet.type() != BlockchainMessage::Transfer::rtt)
        throw std::runtime_error("Unknown object typeid to insert: " + std::to_string(packet.type()));

    vector<char> packet_vec = packet.save();
    auto str_hash = meshpp::hash(packet_vec.begin(), packet_vec.end());
    string file_name(str_hash + ".tpool");

    boost::filesystem::ofstream fl;
    fl.open(m_pimpl->m_path / file_name,
        std::ios_base::binary |
        std::ios_base::trunc);

    if (!fl)
        throw std::runtime_error("Cannot create file: " + file_name);

    fl.write(&packet_vec.front(), packet_vec.size());

    m_pimpl->m_index->keys.push_back(str_hash);
    std::sort(m_pimpl->m_index->keys.begin(), m_pimpl->m_index->keys.end());
    m_pimpl->m_index.save();
}

bool transaction_pool::at(std::string const& key, beltpp::packet& transaction) const
{
    bool found =
            std::binary_search(m_pimpl->m_index.as_const()->keys.begin(),
                               m_pimpl->m_index.as_const()->keys.end(),
                               key);
    if (false == found)
        return false;

    string file_name(key + ".tpool");

    auto path = m_pimpl->m_path / file_name;
    std::istream_iterator<char> end, begin;
    boost::filesystem::ifstream fl;
    meshpp::load_file(path, fl, begin, end);

    beltpp::detail::session_special_data ssd;
    ::beltpp::detail::pmsg_all pmsgall(size_t(-1),
                                       ::beltpp::void_unique_ptr(nullptr, [](void*){}),
                                       nullptr);
    if (fl && begin != end)
    {
        string file_all(begin, end);
        beltpp::iterator_wrapper<char const> it_begin(file_all.begin());
        beltpp::iterator_wrapper<char const> it_end(file_all.end());
        pmsgall = BlockchainMessage::message_list_load(it_begin, it_end, ssd, nullptr);
    }

    if (pmsgall.rtt == 0 ||
        pmsgall.pmsg == nullptr)
    {
        throw std::runtime_error("transaction_pool::at(): " + path.string());
    }
    else
    {
        transaction.set(pmsgall.rtt,
                        std::move(pmsgall.pmsg),
                        pmsgall.fsaver);
    }

    return true;
}

}
