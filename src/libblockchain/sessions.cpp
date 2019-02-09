#include "sessions.hpp"
#include "common.hpp"
#include "message.hpp"
#include "communication_rpc.hpp"
#include "node_internals.hpp"

#include <mesh.pp/cryptoutility.hpp>
#include <belt.pp/utility.hpp>

#include <iostream>
#include <algorithm>

namespace chrono = std::chrono;
using chrono::system_clock;
using chrono::steady_clock;
using std::string;

namespace publiqpp
{

session_action_connections::session_action_connections(beltpp::socket& sk,
                                                       beltpp::ip_address const& _address)
    : meshpp::session_action()
    , need_to_drop(false)
    , psk(&sk)
    , address(_address)
{}

session_action_connections::~session_action_connections()
{
    if (need_to_drop)
        psk->send(peerid_update, beltpp::isocket_drop());
}

void session_action_connections::initiate()
{
    auto peerids = psk->open(address);
    if (peerids.size() != 1)
        throw std::logic_error("socket open by ip address must give just one peerid");

    peerid_update = peerids.front();
    expected_next_package_type = beltpp::isocket_join::rtt;
}

bool session_action_connections::process(beltpp::packet&& package)
{
    bool code = true;

    if (expected_next_package_type == package.type() &&
        expected_next_package_type != size_t(-1))
    {
        switch (package.type())
        {
        case beltpp::isocket_join::rtt:
            std::cout << "session 1 - join" << std::endl;
            need_to_drop = true;
            completed = true;
            expected_next_package_type = size_t(-1);
            break;
        default:
            assert(false);
            break;
        }
    }
    else
    {
        switch (package.type())
        {
        case beltpp::isocket_drop::rtt:
            errored = true;
            need_to_drop = false;
            std::cout << "session 1 - join" << std::endl;
            break;
        case beltpp::isocket_protocol_error::rtt:
            errored = true;
            std::cout << "session 1 - protocol error" << std::endl;
            if (need_to_drop)
            {
                psk->send(peerid_update, beltpp::isocket_drop());
                need_to_drop = false;
            }
            break;
        case beltpp::isocket_open_error::rtt:
            std::cout << "session 1 - open error" << std::endl;
            errored = true;
            break;
        case beltpp::isocket_open_refused::rtt:
            std::cout << "session 1 - open refused" << std::endl;
            errored = true;
            break;
        default:
            code = false;
            break;
        }
    }

    return code;
}

session_action_signatures::session_action_signatures(beltpp::socket& sk,
                                                     nodeid_service& service,
                                                     std::string const& _nodeid,
                                                     beltpp::ip_address const& _address)
    : session_action()
    , psk(&sk)
    , pnodeid_service(&service)
    , nodeid(_nodeid)
    , address(_address)
{}

session_action_signatures::~session_action_signatures()
{
    if (completed &&
        false == errored)
        erase(true, false);
    else if (expected_next_package_type != size_t(-1))
        erase(false, false);
}

void session_action_signatures::initiate()
{
    psk->send(parent->peerid, BlockchainMessage::Ping());
    expected_next_package_type = BlockchainMessage::Pong::rtt;
}

bool session_action_signatures::process(beltpp::packet&& package)
{
    bool code = true;

    if (expected_next_package_type == package.type() &&
        expected_next_package_type != size_t(-1))
    {
        switch (package.type())
        {
        case BlockchainMessage::Pong::rtt:
        {
            std::cout << "session 2" << std::endl;
            BlockchainMessage::Pong msg;
            std::move(package).get(msg);

            auto diff = system_clock::from_time_t(msg.stamp.tm) - system_clock::now();
            string message = msg.nodeid + ::beltpp::gm_time_t_to_gm_string(msg.stamp.tm);

            if ((chrono::seconds(-30) <= diff && diff < chrono::seconds(30)) &&
                meshpp::verify_signature(msg.nodeid, message, msg.signature) &&
                nodeid == msg.nodeid)
            {
                std::cout << "session 2 - ok verify" << std::endl;

                erase(true, true);
            }
            else
            {
                errored = true;
                std::cout << "session 2 - fail verify" << std::endl;

                erase(false, false);
            }

            completed = true;
            expected_next_package_type = size_t(-1);
            break;
        }
        default:
            assert(false);
            break;
        }
    }
    else
    {
        code = false;
    }

    return code;
}

void session_action_signatures::erase(bool success, bool verified)
{
    auto it = pnodeid_service->nodeids.find(nodeid);
    if (it == pnodeid_service->nodeids.end())
    {
        assert(false);
        throw std::logic_error("session_action_signatures::process "
                               "cannot find the expected entry");
    }
    else
    {
        auto& array = it->second.addresses;
        auto it_end = std::remove_if(array.begin(), array.end(),
        [this, success](nodeid_address_unit const& unit)
        {
            if (success)
                return unit.address != address;
            else
                return unit.address == address;
        });
        array.erase(it_end, array.end());

        if (success)
        {
            assert(array.size() == 1);
            array.front().verified = verified;
        }
    }
}

session_action_broadcast_address_info::session_action_broadcast_address_info(detail::node_internals* _pimpl,
                                                                             meshpp::p2psocket::peer_id const& _source_peer,
                                                                             BlockchainMessage::Broadcast&& _msg)
    : session_action()
    , pimpl(_pimpl)
    , source_peer(_source_peer)
    , msg(std::move(_msg))
{}

session_action_broadcast_address_info::~session_action_broadcast_address_info()
{}

void session_action_broadcast_address_info::initiate()
{
    std::cout << "session 3 - broadcasting" << std::endl;
    broadcast_message(std::move(msg),
                      pimpl->m_ptr_p2p_socket->name(),
                      source_peer,
                      false,
                      nullptr,
                      pimpl->m_p2p_peers,
                      pimpl->m_ptr_p2p_socket.get());

    expected_next_package_type = size_t(-1);
    completed = true;
}

bool session_action_broadcast_address_info::process(beltpp::packet&&)
{
    return false;
}

}

