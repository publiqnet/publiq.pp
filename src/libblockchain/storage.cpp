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
        : map("storage", path, 10000, detail::get_putl())
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

bool storage::put(BlockchainMessage::StorageFile&& file, string& uri)
{
    bool code = false;
    uri = meshpp::hash(file.data);
    file.data = meshpp::to_base64(file.data);
    beltpp::on_failure guard([this]
    {
        m_pimpl->map.discard();
    });

    if (m_pimpl->map.insert(uri, file))
        code = true;

    m_pimpl->map.save();

    guard.dismiss();
    m_pimpl->map.commit();
    return code;
}

bool storage::get(string const& uri, BlockchainMessage::StorageFile& file)
{
    auto keys = m_pimpl->map.keys();
    auto it = keys.find(uri);
    if (it == keys.end())
        return false;

    file = std::move(m_pimpl->map.at(uri));
    file.data = meshpp::from_base64(file.data);
    m_pimpl->map.discard();

    return true;
}

bool storage::remove(string const& uri)
{
    auto keys = m_pimpl->map.keys();
    auto it = keys.find(uri);
    if (it == keys.end())
        return false;

    beltpp::on_failure guard([this]
    {
        m_pimpl->map.discard();
    });
    m_pimpl->map.erase(uri);
    m_pimpl->map.save();

    guard.dismiss();
    m_pimpl->map.commit();

    return true;
}


}
