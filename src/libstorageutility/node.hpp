#pragma once

#include "global.hpp"
#include "types.hpp"

#include <belt.pp/ilog.hpp>
#include <belt.pp/isocket.hpp>

#include <mesh.pp/cryptoutility.hpp>

#include <boost/filesystem/path.hpp>

#include <memory>
#include <string>
#include <chrono>
#include <map>

namespace storage_utilitypp
{
namespace detail
{
    class node_internals;
}

class STORAGEUTILITYSHARED_EXPORT node
{
public:
    node(beltpp::ip_address const& rpc_bind_to_address,
         beltpp::ilog* plogger_node);
    node(node&& other) noexcept;
    ~node();

    void wake();
    bool run();

private:
    std::unique_ptr<detail::node_internals> m_pimpl;
};

}

