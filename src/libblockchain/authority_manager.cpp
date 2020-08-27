#include "authority_manager.hpp"
#include "common.hpp"
#include "exception.hpp"

#include <mesh.pp/fileutility.hpp>

using std::unordered_set;
using std::string;
namespace filesystem = boost::filesystem;
namespace publiqpp
{
namespace detail
{
class authority_manager_impl
{
public:
    authority_manager_impl(filesystem::path const& path_authority_store)
        : m_authority_store("authorities", path_authority_store, 10000, get_putl_types())
    {}

    meshpp::map_loader<StorageTypes::AccountAuthorizations> m_authority_store;
};
}

bool authority_manager::check_authority(string const& address, string const& authority, size_t action_id) const
{
    auto auth_record = get_record(address, authority);

    if (auth_record.action_ids.count(action_id))
        return false == auth_record.default_full;
    
    return auth_record.default_full;
}

std::string authority_manager::get_authority(string const& address, size_t/* action_id*/) const
{
    return address;
}

string authority_manager::find_authority(unordered_set<string> const& authorities,
                                         string const& address,
                                         size_t action_id) const
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
        if (check_authority(address, authority, action_id))
            return authority;
    }

    return string();
}


StorageTypes::AccountAuthorization authority_manager::get_record(string const& address, string const& authority) const
{
    if (false == m_pimpl->m_authority_store.contains(address))
    {
        if (authority == address)
        {
            StorageTypes::AccountAuthorization address_record;
            address_record.default_full = true;

            return address_record;
        }
        else
        {
            StorageTypes::AccountAuthorization address_record;
            address_record.default_full = false;

            return address_record;
        }
    }

    StorageTypes::AccountAuthorizations const& auths = m_pimpl->m_authority_store.as_const().at(address);

    auto it_authority = auths.authorizations.find(authority);

    if (it_authority == auths.authorizations.end())
    {
        if (authority == address)
        {
            throw std::logic_error("authority_manager::get_record: authority == address");

            // StorageTypes::AccountAuthorization address_record;
            // address_record.default_full = true;

            // return address_record;
        }
        else
        {
            StorageTypes::AccountAuthorization address_record;
            address_record.default_full = false;

            return address_record;
        }
    }

    return it_authority->second;
}

void authority_manager::set_record(string const& address, string const& authority, StorageTypes::AccountAuthorization const& auth_record)
{
    if (false == m_pimpl->m_authority_store.contains(address))
        throw std::logic_error("authority_manager::set_record: false == m_pimpl->m_authority_store.contains(address)");

    StorageTypes::AccountAuthorizations& auths = m_pimpl->m_authority_store.at(address);

    auto it_authority = auths.authorizations.find(authority);
    if (it_authority == auths.authorizations.end())
        throw std::logic_error("authority_manager::set_record: it_authority == auths.authorizations.end()");

    it_authority->second = auth_record;
}

void authority_manager::smart_create_dummy_record(string const& address, string const& authority)
{
    if (false == m_pimpl->m_authority_store.contains(address))
    {
        StorageTypes::AccountAuthorization address_record;
        address_record.default_full = true;

        StorageTypes::AccountAuthorizations auths;
        auths.authorizations[address] = address_record;

        m_pimpl->m_authority_store.insert(address, auths);
    }

    if (address != authority)
    {
        if (false == m_pimpl->m_authority_store.contains(address))
            throw std::logic_error("authority_manager::smart_create_dummy_record: false == m_pimpl->m_authority_store.contains(address)");
        
        StorageTypes::AccountAuthorizations& auths = m_pimpl->m_authority_store.at(address);

        StorageTypes::AccountAuthorization authority_record;
        authority_record.default_full = false;

        auto insert_res = auths.authorizations.insert({authority, authority_record});
        B_UNUSED(insert_res);
    }
}

void authority_manager::smart_cleanup_dummy_record(string const& address, string const& authority)
{
    if (address != authority)
    {
        if (false == m_pimpl->m_authority_store.contains(address))
            throw std::logic_error("authority_manager::smart_cleanup_dummy_record: false == m_pimpl->m_authority_store.contains(address)");

        StorageTypes::AccountAuthorizations& auths = m_pimpl->m_authority_store.at(address);

        auto it_authority = auths.authorizations.find(authority);
        if (it_authority == auths.authorizations.end())
            throw std::logic_error("authority_manager::smart_cleanup_dummy_record: it_authority == auths.authorizations.end()");

        if (it_authority->second.default_full == false &&
            it_authority->second.action_ids.empty())
            auths.authorizations.erase(it_authority);
    }

    if (m_pimpl->m_authority_store.contains(address))
    {
        StorageTypes::AccountAuthorizations& auths = m_pimpl->m_authority_store.at(address);

        auto it_address = auths.authorizations.find(address);
        if (it_address == auths.authorizations.end())
            throw std::logic_error("authority_manager::smart_cleanup_dummy_record: it_address == auths.authorizations.end()");

        if (it_address->second.default_full == true &&
            it_address->second.action_ids.empty() &&
            auths.authorizations.size() == 1)
            m_pimpl->m_authority_store.erase(address);
    }
}

void authority_manager::save()
{
    m_pimpl->m_authority_store.save();
}

void authority_manager::commit() noexcept
{
    m_pimpl->m_authority_store.commit();
}

void authority_manager::discard() noexcept
{
    m_pimpl->m_authority_store.discard();
}

void authority_manager::clear()
{
    m_pimpl->m_authority_store.clear();
}


authority_manager::authority_manager(filesystem::path const& path_authority_store)
    : m_pimpl(new detail::authority_manager_impl(path_authority_store))
{}

authority_manager::~authority_manager() = default;

}