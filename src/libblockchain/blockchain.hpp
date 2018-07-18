#pragma once

#include "global.hpp"
#include "message.hpp"

#include <boost/filesystem/path.hpp>

#include <vector>
#include <memory>

namespace publiqpp
{

namespace detail
{
class blockchain_internals;
}

class blockchain
{
public:
    blockchain(boost::filesystem::path const& fs_blockchain);
    ~blockchain();

    size_t length() const;
    void insert(beltpp::packet const& packet);
    bool at(uint64_t number, beltpp::packet& signed_block) const;
    void remove_last_block();
private:
    std::unique_ptr<detail::blockchain_internals> m_pimpl;
};

}
