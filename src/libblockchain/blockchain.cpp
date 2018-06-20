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

    using number_loader = meshpp::file_loader<Data::Number, &Data::Number::string_loader, &Data::Number::string_saver>;
    using number_locked_loader = meshpp::file_locker<number_loader>;

    number_locked_loader m_length;
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
