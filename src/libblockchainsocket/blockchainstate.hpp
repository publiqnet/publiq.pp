#pragma once

#include "global.hpp"

#include <boost/filesystem/path.hpp>

#include <vector>
#include <memory>

namespace publiqpp
{

class blockchainstate
{
public:
    virtual ~blockchainstate() {};
};

using blockchainstate_ptr = beltpp::t_unique_ptr<blockchainstate>;
blockchainstate_ptr getblockchainstate(boost::filesystem::path const& fs_blockchain);
}
