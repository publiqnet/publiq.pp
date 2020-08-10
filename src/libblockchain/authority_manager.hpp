#pragma once

#include "global.hpp"

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

    bool check_authority(std::string const& address, std::string const& authority, size_t tx_type) const;
    bool check_miner_authority(std::string const& address, std::string const& authority) const;
    std::string get_authority(std::string const& address, size_t tx_type) const;

    std::string find_authority(std::unordered_set<std::string> const& authorities,
                               std::string const& address,
                               size_t tx_type) const;
    
private:
    std::unique_ptr<detail::authority_manager_impl> m_pimpl;
};
}