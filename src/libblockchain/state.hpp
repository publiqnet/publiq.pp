#pragma once

#include "global.hpp"

#include "blockchain.hpp"

#include <boost/filesystem/path.hpp>

#include <memory>

namespace publiqpp
{
namespace detail
{
class state_internals;
}
class state
{
public:
    state(boost::filesystem::path const& fs_blockchain);
    ~state();

    publiqpp::blockchain& blockchain();
private:
    std::unique_ptr<detail::state_internals> m_pimpl;
};
}
