#include "storage.hpp"
#include "common.hpp"

#include "message.hpp"

#include <mesh.pp/fileutility.hpp>
#include <mesh.pp/cryptoutility.hpp>

#include <string>

namespace filesystem = boost::filesystem;
using std::string;

namespace publiqpp
{

namespace detail
{

class storage_internals
{
public:
    storage_internals(filesystem::path const& path)
        : map("storage", path, detail::get_putl())
    {}

    meshpp::map_loader<BlockchainMessage::StorageFile> map;
};

}

storage::storage(boost::filesystem::path const& fs_storage)
    : m_pimpl(new detail::storage_internals(fs_storage))
{
#if 0
    meshpp::map_loader<BlockchainMessage::StorageFile> map1("1storage", m_pimpl->m_path, detail::get_putl());
    meshpp::map_loader<BlockchainMessage::StorageFile> map2("2storage", m_pimpl->m_path, detail::get_putl());
#endif
#if 0
    BlockchainMessage::StorageFile temp;
    temp.mime_type = "app/text";

    for (int i = 0; i < 20000; ++i)
    {
        temp.data = std::to_string(i);
        map1.insert(temp.data, temp);
    }
    map1.save();
    map2.discard();
#endif
#if 0
    for (auto const& key : map1.keys())
    {
        map2.insert(key, map1.at(key));
    }
    map1.discard();
    map2.save();
#endif

}
storage::~storage()
{}

string storage::put(BlockchainMessage::StorageFile&& file)
{
    string hash;
    hash = meshpp::hash(file.data);
    //file.data = meshpp::to_base64(file.data);
    m_pimpl->map.insert(hash, file);

    m_pimpl->map.save();
    return hash;
}

bool storage::get(string const& hash, BlockchainMessage::StorageFile& file)
{
    auto keys = m_pimpl->map.keys();
    auto it = keys.find(hash);
    if (it == keys.end())
        return false;

    file = std::move(m_pimpl->map.at(hash));
    //file.data = meshpp::from_base64(file.data);
    m_pimpl->map.discard();

    return true;
}


}
