#pragma once

#include "common.hpp"
#include "http.hpp"

#include "state.hpp"
#include "storage.hpp"
#include "action_log.hpp"
#include "blockchain.hpp"
#include "session_manager.hpp"

#include <belt.pp/event.hpp>
#include <belt.pp/socket.hpp>
#include <belt.pp/packet.hpp>
#include <belt.pp/utility.hpp>
#include <belt.pp/scope_helper.hpp>
#include <belt.pp/message_global.hpp>
#include <belt.pp/timer.hpp>

#include <mesh.pp/p2psocket.hpp>
#include <mesh.pp/cryptoutility.hpp>

#include <boost/filesystem/path.hpp>

#include <map>
#include <chrono>
#include <memory>

using namespace libblockchain;
using namespace StorageMessage;
namespace filesystem = boost::filesystem;

using beltpp::ip_address;
using beltpp::ip_destination;
using beltpp::socket;
using beltpp::packet;
using peer_id = socket::peer_id;

using std::map;
using std::pair;
using std::string;
using std::vector;
using std::unique_ptr;
using std::unordered_set;
using std::unordered_map;

namespace chrono = std::chrono;
using chrono::system_clock;
using chrono::steady_clock;

namespace publiqpp
{
    using rpc_storage_sf = beltpp::socket_family_t<&http::message_list_load<&StorageMessage::message_list_load>>;

namespace detail
{

class storage_node_internals
{
public:
    storage_node_internals(
        ip_address const& rpc_bind_to_address,
        filesystem::path const& fs_storage,
        beltpp::ilog* _plogger_storage_node,
        bool log_enabled)
        : plogger_storage_node(_plogger_storage_node)
        , m_ptr_eh(new beltpp::event_handler())
        , m_ptr_rpc_socket(new beltpp::socket(
                               beltpp::getsocket<rpc_storage_sf>(*m_ptr_eh)
                               ))
        , m_rpc_bind_to_address(rpc_bind_to_address)
        , m_storage(fs_storage)
    {
        m_ptr_eh->set_timer(chrono::seconds(EVENT_TIMER));

        if (false == rpc_bind_to_address.local.empty())
            m_ptr_rpc_socket->listen(rpc_bind_to_address);

        m_ptr_eh->add(*m_ptr_rpc_socket);

//        if(m_node_type == NodeType::storage)
//            m_stat_counter.init(meshpp::hash(signed_block.block_details.to_string()));
    }

    void writeln_node(string const& value)
    {
        if (plogger_storage_node)
            plogger_storage_node->message(value);
    }

    void writeln_node_warning(string const& value)
    {
        if (plogger_storage_node)
            plogger_storage_node->warning(value);
    }

    beltpp::ilog* plogger_storage_node;
    unique_ptr<beltpp::event_handler> m_ptr_eh;
    unique_ptr<beltpp::socket> m_ptr_rpc_socket;

    beltpp::ip_address m_rpc_bind_to_address;

    publiqpp::storage m_storage;
};

}
}
