#pragma once

#include "common.hpp"
#include "http.hpp"

#include "state.hpp"
#include "storage.hpp"
#include "action_log.hpp"
#include "blockchain.hpp"
#include "node.hpp"

#include <belt.pp/ievent.hpp>
#include <belt.pp/socket.hpp>
#include <belt.pp/packet.hpp>
#include <belt.pp/utility.hpp>
#include <belt.pp/scope_helper.hpp>
#include <belt.pp/message_global.hpp>
#include <belt.pp/timer.hpp>
#include <belt.pp/direct_stream.hpp>

#include <mesh.pp/p2psocket.hpp>
#include <mesh.pp/cryptoutility.hpp>

#include <boost/filesystem/path.hpp>

#include <map>
#include <chrono>
#include <memory>
#include <list>
#include <utility>
#include <unordered_set>

using namespace BlockchainMessage;
namespace filesystem = boost::filesystem;

using beltpp::ip_address;
using beltpp::ip_destination;
using beltpp::socket;
using beltpp::packet;
using peer_id = socket::peer_id;

using std::map;
using std::pair;
using std::string;
using std::list;
using std::unique_ptr;
using std::unordered_set;
using std::unordered_map;

namespace chrono = std::chrono;
using chrono::system_clock;
using chrono::steady_clock;

namespace publiqpp
{
    using rpc_storage_sf = beltpp::socket_family_t<&http::message_list_load<&BlockchainMessage::message_list_load>>;

namespace detail
{

class storage_node_internals
{
public:
    storage_node_internals(
        config& ref_config,
        filesystem::path const& fs_storage,
        beltpp::ilog* _plogger_storage_node,
        beltpp::direct_channel& channel)
        : plogger_storage_node(_plogger_storage_node)
        , pconfig(&ref_config)
        , m_ptr_eh(beltpp::libsocket::construct_event_handler())
        , m_ptr_rpc_socket(beltpp::libsocket::getsocket<rpc_storage_sf>(*m_ptr_eh))
        , m_ptr_direct_stream(beltpp::construct_direct_stream(storage_peerid, *m_ptr_eh, channel))
        , m_storage(fs_storage)
    {
        m_ptr_eh->set_timer(chrono::seconds(EVENT_TIMER));

        if (false == pconfig->get_slave_bind_to_address().local.empty())
            m_ptr_rpc_socket->listen(pconfig->get_slave_bind_to_address());

        m_ptr_eh->add(*m_ptr_rpc_socket);
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

    meshpp::public_key front_public_key() const
    {
        return pconfig->get_public_key();
    }

    meshpp::private_key front_private_key() const
    {
        return pconfig->get_key();
    }

    beltpp::ilog* plogger_storage_node;
    config* pconfig;
    beltpp::event_handler_ptr m_ptr_eh;
    beltpp::socket_ptr m_ptr_rpc_socket;
    beltpp::stream_ptr m_ptr_direct_stream;

    publiqpp::storage m_storage;

    unordered_set<string> m_verified_channels;
    unordered_map<string, BlockchainMessage::StorageFileRedirect> m_redirects;

    event_queue_manager m_event_queue;
};

}
}
