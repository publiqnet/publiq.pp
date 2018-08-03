#pragma once

#include "http.hpp"

#include "state.hpp"
#include "storage.hpp"
#include "action_log.hpp"
#include "blockchain.hpp"

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

using namespace BlockchainMessage;

namespace filesystem = boost::filesystem;

using beltpp::ip_address;
using beltpp::ip_destination;
using beltpp::socket;
using beltpp::packet;
using peer_id = socket::peer_id;

using std::pair;
using std::string;
using std::vector;
using std::unique_ptr;
using std::unordered_set;
using std::unordered_map;

namespace chrono = std::chrono;
using chrono::system_clock;
using chrono::steady_clock;

//  MSVS does not instansiate template function only because its address
//  is needed, so let's force it
template beltpp::void_unique_ptr beltpp::new_void_unique_ptr<Error>();
template beltpp::void_unique_ptr beltpp::new_void_unique_ptr<Join>();
template beltpp::void_unique_ptr beltpp::new_void_unique_ptr<Drop>();

namespace publiqpp
{

using p2p_sf = meshpp::p2psocket_family_t<
    Error::rtt,
    Join::rtt,
    Drop::rtt,
    &beltpp::new_void_unique_ptr<Error>,
    &beltpp::new_void_unique_ptr<Join>,
    &beltpp::new_void_unique_ptr<Drop>,
    &Error::pvoid_saver,
    &Join::pvoid_saver,
    &Drop::pvoid_saver
>;

using rpc_sf = beltpp::socket_family_t<
    Error::rtt,
    Join::rtt,
    Drop::rtt,
    &beltpp::new_void_unique_ptr<Error>,
    &beltpp::new_void_unique_ptr<Join>,
    &beltpp::new_void_unique_ptr<Drop>,
    &Error::pvoid_saver,
    &Join::pvoid_saver,
    &Drop::pvoid_saver,
    &http::message_list_load
>;

namespace detail
{

class packet_and_expiry
{
public:
    beltpp::packet packet;
    size_t expiry;
};

inline
beltpp::void_unique_ptr get_putl()
{
    beltpp::message_loader_utility utl;
    BlockchainMessage::detail::extension_helper(utl);

    auto ptr_utl =
        beltpp::new_void_unique_ptr<beltpp::message_loader_utility>(std::move(utl));

    return ptr_utl;
}

class node_internals
{
public:
    node_internals(ip_address const& rpc_bind_to_address,
        ip_address const& p2p_bind_to_address,
        std::vector<ip_address> const& p2p_connect_to_addresses,
        filesystem::path const& fs_blockchain,
        filesystem::path const& fs_action_log,
        filesystem::path const& fs_storage,
        filesystem::path const& fs_transaction_pool,
        filesystem::path const& fs_state,
        beltpp::ilog* _plogger_p2p,
        beltpp::ilog* _plogger_node,
        meshpp::private_key pv_key)
        : plogger_p2p(_plogger_p2p)
        , plogger_node(_plogger_node)
        , m_ptr_eh(new beltpp::event_handler())
        , m_ptr_p2p_socket(new meshpp::p2psocket(
            meshpp::getp2psocket<p2p_sf>(*m_ptr_eh,
                p2p_bind_to_address,
                p2p_connect_to_addresses,
                get_putl(),
                _plogger_p2p)
        ))
        , m_ptr_rpc_socket(new beltpp::socket(
            beltpp::getsocket<rpc_sf>(*m_ptr_eh)
        ))
        , m_sync_timer()
        , m_check_timer()

        , m_blockchain(fs_blockchain)
        , m_action_log(fs_action_log)
        , m_storage(fs_storage)
        , m_transaction_pool(fs_transaction_pool)
        , m_state(fs_state)
        , private_key(pv_key)
    {
        sync_peerid = "Ice Age"; // will triger initial sync
        m_sync_timer.set(chrono::seconds(SYNC_TIMER));
        m_check_timer.set(chrono::seconds(CHECK_TIMER));
        m_ptr_eh->set_timer(chrono::seconds(EVENT_TIMER));

        m_ptr_rpc_socket->listen(rpc_bind_to_address);

        m_ptr_eh->add(*m_ptr_rpc_socket);
        m_ptr_eh->add(*m_ptr_p2p_socket);
    }

    void write_p2p(string const& value)
    {
        if (plogger_p2p)
            plogger_p2p->message_no_eol(value);
    }

    void writeln_p2p(string const& value)
    {
        if (plogger_p2p)
            plogger_p2p->message(value);
    }

    void write_node(string const& value)
    {
        if (plogger_node)
            plogger_node->message_no_eol(value);
    }

    void writeln_node(string const& value)
    {
        if (plogger_node)
            plogger_node->message(value);
    }

    void add_peer(socket::peer_id const& peerid)
    {
        pair<unordered_set<socket::peer_id>::iterator, bool> result =
            m_p2p_peers.insert(peerid);

        if (result.second == false)
            throw std::runtime_error("p2p peer already exists: " + peerid);
    }

    void remove_peer(socket::peer_id const& peerid)
    {
        clear_sync_state(peerid);
        reset_stored_request(peerid);
        if (0 == m_p2p_peers.erase(peerid))
            throw std::runtime_error("p2p peer not found to remove: " + peerid);
    }

    bool find_stored_request(socket::peer_id const& peerid, beltpp::packet& packet)
    {
        auto it = m_stored_requests.find(peerid);
        if (it != m_stored_requests.end())
        {
            BlockchainMessage::detail::assign_packet(packet, it->second.packet);
            return true;
        }

        return false;
    }

    void reset_stored_request(beltpp::isocket::peer_id const& peerid)
    {
        m_stored_requests.erase(peerid);
    }

    void store_request(socket::peer_id const& peerid, beltpp::packet const& packet)
    {
        detail::packet_and_expiry pck;
        BlockchainMessage::detail::assign_packet(pck.packet, packet);
        pck.expiry = 2;
        auto res = m_stored_requests.insert(std::make_pair(peerid, std::move(pck)));
        if (false == res.second)
            throw std::runtime_error("only one request is supported at a time");
    }

    bool sync_free()
    {
        if (sync_peerid.empty())
            return true;

        return false;
    }

    bool sync_timeout()
    {
        system_clock::time_point current_time_point = system_clock::now();
        system_clock::time_point previous_time_point = system_clock::from_time_t(sync_time.tm);

        chrono::seconds diff_seconds = chrono::duration_cast<chrono::seconds>(current_time_point - previous_time_point);

        if (diff_seconds.count() >= SYNC_STEP_TIMEOUT)
        {
            clear_sync_state(sync_peerid);

            return true;
        }

        return false;
    }

    void update_sync_time()
    {
        sync_time.tm = system_clock::to_time_t(system_clock::now());
    }

    void clear_sync_state(beltpp::isocket::peer_id peerid)
    {
        if (peerid == sync_peerid)
        {
            sync_peerid.clear();
            sync_block_vector.clear();
            sync_header_vector.clear();
            sync_response_vector.clear();
        }
    }

    void new_sync_request()
    {
        // clear state
        clear_sync_state(sync_peerid);

        // send new request to all peers
        BlockHeader block_header;
        m_blockchain.header(block_header);
        beltpp::isocket* psk = m_ptr_p2p_socket.get();

        SyncRequest sync_request;
        sync_request.block_number = block_header.block_number;
        sync_request.consensus_sum = block_header.consensus_sum;

        for (auto& it : m_p2p_peers)
        {
            // temp solution
            packet stored_packet;

            if (!find_stored_request(it, stored_packet) || (stored_packet.type() == SyncRequest::rtt))
            {
                psk->send(it, sync_request);
                reset_stored_request(it);
                store_request(it, sync_request);
            }
        }

        update_sync_time();
    }

    std::vector<beltpp::isocket::peer_id> do_step()
    {
        vector<beltpp::isocket::peer_id> result;

        for (auto& key_value : m_stored_requests)
        {
            if (0 == key_value.second.expiry)
                result.push_back(key_value.first);

            --key_value.second.expiry;
        }
        return result;
    }

    beltpp::ilog* plogger_p2p;
    beltpp::ilog* plogger_node;
    unique_ptr<beltpp::event_handler> m_ptr_eh;
    unique_ptr<meshpp::p2psocket> m_ptr_p2p_socket;
    unique_ptr<beltpp::socket> m_ptr_rpc_socket;

    beltpp::timer m_sync_timer;
    beltpp::timer m_check_timer;

    publiqpp::blockchain m_blockchain;
    publiqpp::action_log m_action_log;
    publiqpp::storage m_storage;
    publiqpp::transaction_pool m_transaction_pool;
    publiqpp::state m_state;

    unordered_set<beltpp::isocket::peer_id> m_p2p_peers;
    unordered_map<beltpp::isocket::peer_id, packet_and_expiry> m_stored_requests;

    meshpp::private_key private_key;

    BlockchainMessage::ctime sync_time;
    beltpp::isocket::peer_id sync_peerid;
    vector<SignedBlock> sync_block_vector;
    vector<BlockHeader> sync_header_vector;
    vector<std::pair<beltpp::isocket::peer_id, SyncResponse>> sync_response_vector;
};

}
}
