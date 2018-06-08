#pragma once

#include "global.hpp"

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
private:
    std::unique_ptr<detail::blockchain_internals> m_pimpl;
};

}
