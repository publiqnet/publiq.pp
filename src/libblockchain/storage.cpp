#include "storage.hpp"

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
inline
beltpp::void_unique_ptr get_putl()
{
    beltpp::message_loader_utility utl;
    BlockchainMessage::detail::extension_helper(utl);

    auto ptr_utl =
        beltpp::new_void_unique_ptr<beltpp::message_loader_utility>(std::move(utl));

    return ptr_utl;
}

class storage_internals
{
public:
    storage_internals(filesystem::path const& path)
        : map("storage", path, detail::get_putl())
    {

    }

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
{

}

BlockchainMessage::Digest storage::put(BlockchainMessage::StorageFile&& file)
{
    BlockchainMessage::Digest hash;
    hash.base58_hash = meshpp::hash(file.to_string());
    m_pimpl->map.insert(hash.base58_hash, file);

    m_pimpl->map.save();
    return hash;
}

bool storage::get(BlockchainMessage::Digest const& hash, BlockchainMessage::StorageFile& file)
{
    auto keys = m_pimpl->map.keys();
    auto it = keys.find(hash.base58_hash);
    if (it == keys.end())
        return false;

    file = std::move(m_pimpl->map.at(hash.base58_hash));
    m_pimpl->map.discard();

    return true;
}


}
