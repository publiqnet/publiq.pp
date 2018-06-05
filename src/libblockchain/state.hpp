#pragma once

#include "global.hpp"

#include <boost/filesystem/path.hpp>

#include <vector>
#include <memory>

namespace publiqpp
{

class state
{
public:
    virtual ~state() {};
};

using state_ptr = beltpp::t_unique_ptr<state>;
state_ptr getstate(boost::filesystem::path const& fs_blockchain);
}
