#include "blockchain.hpp"

#include "data.hpp"

#include <mesh.pp/fileutility.hpp>

namespace filesystem = boost::filesystem;

namespace publiqpp
{

namespace detail
{
class blockchain_internals
{
public:
    blockchain_internals(filesystem::path const& path)
        : m_length(path / "length.txt")
    {

    }

    using length_loader = meshpp::file_loader<Data::Length, &Data::Length::string_loader, &Data::Length::string_saver>;
    using length_locked_loader = meshpp::file_locker<length_loader>;

    length_locked_loader m_length;
};
}

blockchain::blockchain(boost::filesystem::path const& fs_blockchain)
    : m_pimpl(new detail::blockchain_internals(fs_blockchain))
{

}
blockchain::~blockchain()
{

}

size_t blockchain::length() const
{
    return m_pimpl->m_length->value;
}
}
