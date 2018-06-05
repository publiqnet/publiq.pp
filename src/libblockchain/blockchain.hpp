#pragma once

#include "global.hpp"

#include <boost/filesystem/path.hpp>

#include <vector>
#include <memory>

namespace publiqpp
{

class blockchain
{
public:
    virtual ~blockchain() {};
};

using blockchain_ptr = beltpp::t_unique_ptr<blockchain>;
blockchain_ptr getblockchain(boost::filesystem::path const& fs_blockchain);
}
