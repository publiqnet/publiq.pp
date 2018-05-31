#include "blockchain.hpp"

#include "data.hpp"

#include <mesh.pp/fileutility.hpp>

namespace filesystem = boost::filesystem;

class blockchain_ex : public publiqpp::blockchain
{
public:
    blockchain_ex(filesystem::path const& path)
        : m_length(path / "length.txt")
    {
    }

    virtual ~blockchain_ex() {}

private:
    using length_loader = meshpp::file_loader<Data::Length, &Data::Length::string_loader, &Data::Length::string_saver>;
    using length_locked_loader = meshpp::file_locker<length_loader>;

    length_locked_loader m_length;
};

namespace publiqpp
{
blockchain_ptr getblockchain(filesystem::path const& path)
{
    return beltpp::new_dc_unique_ptr<blockchain, blockchain_ex>(path);
}
}
