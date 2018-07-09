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

    std::vector<std::string> keys() const;

    void insert(beltpp::packet const& packet);
    bool at(std::string const& key, beltpp::packet& transaction) const;
private:
    std::unique_ptr<detail::transaction_pool_internals> m_pimpl;
};

}
