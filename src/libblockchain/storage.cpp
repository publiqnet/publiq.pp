#include "storage.hpp"

#include "message.hpp"

#include <mesh.pp/fileutility.hpp>

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
        : m_path(path)
    {

    }

    filesystem::path m_path;
};

inline
beltpp::void_unique_ptr get_putl()
{
    beltpp::message_loader_utility utl;
    BlockchainMessage::detail::extension_helper(utl);

    auto ptr_utl =
        beltpp::new_void_unique_ptr<beltpp::message_loader_utility>(std::move(utl));

    return ptr_utl;
}
}

storage::storage(boost::filesystem::path const& fs_storage)
    : m_pimpl(new detail::storage_internals(fs_storage))
{
    meshpp::map_loader<BlockchainMessage::StorageFile> map("storage", m_pimpl->m_path, detail::get_putl());
    BlockchainMessage::StorageFile temp;
    temp.mime_type = "app/text";
    temp.data = "1";
    map.insert("hellothere1", temp);
    temp.mime_type = "app/text";
    temp.data = "2";
    map.insert("hellothere2", temp);
    temp.data = "3";
    map.insert("hellothere2", temp);
    map.insert("bellothere3", temp);

    map.save();
}
storage::~storage()
{

}


}
