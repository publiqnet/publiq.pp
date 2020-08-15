#pragma once

#include "global.hpp"

#include "config.hpp"

#include <belt.pp/ilog.hpp>
#include <belt.pp/isocket.hpp>
#include <belt.pp/direct_stream.hpp>

#include <mesh.pp/cryptoutility.hpp>

#include <boost/filesystem/path.hpp>

#include <memory>

namespace publiqpp
{
class node;
namespace detail
{
    class storage_node_internals;
}

class BLOCKCHAINSHARED_EXPORT storage_node
{
public:
    storage_node(config& ref_config,
                 boost::filesystem::path const& fs_storage,
                 beltpp::ilog* plogger_storage_node,
                 beltpp::direct_channel& channel);
    storage_node(storage_node&& other) noexcept;
    ~storage_node();

    void wake();
    void run(bool& stop);

private:
    std::unique_ptr<detail::storage_node_internals> m_pimpl;
};

}

