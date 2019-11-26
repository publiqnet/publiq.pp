#pragma once

#include "http.hpp"

#include <belt.pp/event.hpp>
#include <belt.pp/socket.hpp>
#include <belt.pp/packet.hpp>
#include <belt.pp/utility.hpp>
#include <belt.pp/scope_helper.hpp>
#include <belt.pp/message_global.hpp>
#include <belt.pp/timer.hpp>

#include <mesh.pp/p2psocket.hpp>
#include <mesh.pp/cryptoutility.hpp>
#include <mesh.pp/sessionutility.hpp>

#include <boost/filesystem/path.hpp>
#include <boost/functional/hash.hpp>

#include <chrono>
#include <memory>
#include <utility>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>

using namespace StorageUtilityMessage;
namespace filesystem = boost::filesystem;

using beltpp::ip_address;
using beltpp::ip_destination;
using beltpp::socket;
using beltpp::packet;
using peer_id = socket::peer_id;

using std::pair;
using std::string;
using std::vector;
using std::map;
using std::unique_ptr;
using std::unordered_set;
using std::unordered_map;

namespace chrono = std::chrono;
using chrono::system_clock;
using chrono::steady_clock;

namespace storage_utility
{
    using rpc_sf = beltpp::socket_family_t<&http::message_list_load<&StorageUtilityMessage::message_list_load>>;

namespace detail
{
class rpc_internals
{
public:
    rpc_internals(ip_address const& rpc_bind_to_address,
                   beltpp::ilog* _plogger_rpc)
        : plogger_rpc(_plogger_rpc)
        , m_ptr_eh(new beltpp::event_handler())
        , m_ptr_rpc_socket(new beltpp::socket(
                               beltpp::getsocket<rpc_sf>(*m_ptr_eh)
                               ))
        , m_rpc_bind_to_address(rpc_bind_to_address)
    {
        m_ptr_eh->set_timer(chrono::seconds(30));

        if (false == rpc_bind_to_address.local.empty())
            m_ptr_rpc_socket->listen(rpc_bind_to_address);

        m_ptr_eh->add(*m_ptr_rpc_socket);
    }

    void writeln_rpc(string const& value)
    {
        if (plogger_rpc)
            plogger_rpc->message(value);
    }

    void writeln_rpc_warning(string const& value)
    {
        if (plogger_rpc)
            plogger_rpc->warning(value);
    }

    beltpp::ilog* plogger_rpc;
    unique_ptr<beltpp::event_handler> m_ptr_eh;
    unique_ptr<beltpp::socket> m_ptr_rpc_socket;

    std::map<string, SignedStorageOrder>  cache_signed_storage_order;

    beltpp::ip_address m_rpc_bind_to_address;
};

}
}
