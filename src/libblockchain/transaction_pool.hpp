#pragma once

#include "global.hpp"

#include <belt.pp/packet.hpp>

#include <boost/filesystem/path.hpp>

#include <vector>
#include <memory>
#include <string>

namespace publiqpp
{

namespace detail
{
class transaction_pool_internals;
}

class transaction_pool
{
public:
    transaction_pool(boost::filesystem::path const& fs_transaction_pool);
    ~transaction_pool();

    void insert(beltpp::packet const& packet);
    bool at(std::string const& key, beltpp::packet& transaction) const;
    bool remove(std::string const& key);

    bool contains(std::string const& key) const;
    void get_keys(std::vector<std::string> &keys);
    void get_amounts(std::string const& key, 
                     std::vector<std::pair<std::string, uint64_t>>& amounts, 
                     bool in_out);
private:
    std::unique_ptr<detail::transaction_pool_internals> m_pimpl;
};

}
