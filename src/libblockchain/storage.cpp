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
}

storage::storage(boost::filesystem::path const& fs_storage)
    : m_pimpl(new detail::storage_internals(fs_storage))
{

}
storage::~storage()
{

}


}
