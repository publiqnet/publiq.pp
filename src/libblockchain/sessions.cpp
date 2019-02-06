#include "sessions.hpp"
#include "common.hpp"
#include "message.hpp"

#include <mesh.pp/cryptoutility.hpp>
#include <belt.pp/utility.hpp>

#include <iostream>

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
    if (expected_next_package_type == size_t(-1))
        return false;

    bool code = true;

    if (expected_next_package_type == package.type())
    {
        switch (package.type())
        {
        case beltpp::isocket_join::rtt:
            std::cout << "session 1" << std::endl;
            need_to_drop = true;
            completed = true;
            expected_next_package_type = size_t(-1);
        default:
            assert(false);
        }
    }
    else
    {
        switch (package.type())
        {
        case beltpp::isocket_protocol_error::rtt:
        case beltpp::isocket_drop::rtt:
            if (need_to_drop)
            {
                psk->send(peerid_update, beltpp::isocket_drop());
                need_to_drop = true;
            }
            break;
        case beltpp::isocket_open_error::rtt:
            break;
        case beltpp::isocket_open_refused::rtt:
            break;
        default:
            code = false;
        }
    }

    return code;
}


session_action_signatures::session_action_signatures(beltpp::socket& sk,
                                                     string const& _nodeid)
    : session_action()
    , psk(&sk)
    , nodeid(_nodeid)
{}

session_action_signatures::~session_action_signatures()
{}

void session_action_signatures::initiate()
{
    psk->send(parent->peerid, BlockchainMessage::Ping());
    expected_next_package_type = BlockchainMessage::Pong::rtt;
}

bool session_action_signatures::process(beltpp::packet&& package)
{
    if (expected_next_package_type == size_t(-1))
        return false;

    bool code = true;

    if (expected_next_package_type == package.type())
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
                std::cout << "session 2 - ok verify" << std::endl;
            else
            {
                errored = true;
                std::cout << "session 2 - fail verify" << std::endl;
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
        code = false;

    return code;
}

}

