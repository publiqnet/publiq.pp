#pragma once

#include "global.hpp"
#include "types.hpp"

#include <boost/filesystem/path.hpp>

#include <utility>
#include <unordered_set>
#include <string>

namespace publiqpp
{
namespace detail
{
    class authority_manager_impl;
}
class authority_manager
{
public:
    authority_manager(boost::filesystem::path const& path_authority_store);
    ~authority_manager();

    bool check_authority(std::string const& address, std::string const& authority, size_t action_id) const;
    std::string get_authority(std::string const& address, size_t action_id) const;

    std::string find_authority(std::unordered_set<std::string> const& authorities,
                               std::string const& address,
                               size_t action_id) const;
    
    StorageTypes::AccountAuthorization get_record(std::string const& address, std::string const& authority) const;
    void set_record(std::string const& address, std::string const& authority, StorageTypes::AccountAuthorization const& auth_record);

    void smart_create_dummy_record(std::string const& address, std::string const& authority);
    void smart_cleanup_dummy_record(std::string const& address, std::string const& authority);

    void save();
    void commit() noexcept;
    void discard() noexcept;
    void clear();
    
private:
    std::unique_ptr<detail::authority_manager_impl> m_pimpl;
};
}