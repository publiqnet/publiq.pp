#include "authority_manager.hpp"
#include "common.hpp"
#include "exception.hpp"
#include "types.hpp"

#include <mesh.pp/fileutility.hpp>

using std::unordered_set;
using std::string;
using namespace BlockchainMessage;
namespace filesystem = boost::filesystem;
namespace publiqpp
{
namespace detail
{
class authority_manager_impl
{
public:
    authority_manager_impl(filesystem::path const& path_authority_store)
        : m_authority_store("authorities", path_authority_store, 10000, get_putl())
    {}

    meshpp::map_loader<Authority> m_authority_store;
};
}

bool authority_manager::check_authority(string const& address, string const& authority, size_t/* tx_type*/) const
{
    return address == authority;
}
bool authority_manager::check_miner_authority(string const& address, string const& authority) const
{
    return address == authority;
}
std::string authority_manager::get_authority(string const& address, size_t/* tx_type*/) const
{
    return address;
}
string authority_manager::find_authority(unordered_set<string> const& authorities,
                                         string const& address,
                                         size_t tx_type) const
{
    if (false == m_pimpl->m_authority_store.contains(address))
    {
        if (authorities.count(address))
            return address;
        else
            return string();
    }

    for (auto const& authority : authorities)
    {
        if (check_authority(address, authority, tx_type))
            return authority;
    }

    return string();
}

authority_manager::authority_manager(filesystem::path const& path_authority_store)
    : m_pimpl(new detail::authority_manager_impl(path_authority_store))
{}

authority_manager::~authority_manager() = default;

}