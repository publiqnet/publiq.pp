#pragma once

#include "common.hpp"
#include "message.hpp"

#include <belt.pp/session_manager.hpp>
#include <belt.pp/isocket.hpp>

#include <mesh.pp/cryptoutility.hpp>

#include <unordered_set>
#include <unordered_map>
#include <chrono>
#include <string>
#include <utility>
#include <iostream>

namespace libblockchain
{

class session_manager
{
    using string = ::std::string;
    template <typename T, typename U>
    using unordered_map = ::std::unordered_map<T, U>;
    template <typename T>
    using unordered_set = ::std::unordered_set<T>;
    using packet = ::beltpp::packet;
    using ip_address = ::beltpp::ip_address;
    using ip_destination = ::beltpp::ip_destination;
    using session = ::beltpp::session<string, ip_address>;
    using system_clock = ::std::chrono::system_clock;
    using seconds = ::std::chrono::seconds;
    using time_point = ::std::chrono::steady_clock::time_point;
    template <typename T, typename U>
    using pair = std::pair<T, U>;
public:
    bool process(string const& peerid, packet&& package, beltpp::isocket* psk)
    {
        auto it = communications.find(peerid);
        if (it == communications.end())
            return false;

        session& s = it->second;

        if (s.expected_package_type == package.type())
        {
            switch (package.type())
            {
            case beltpp::isocket_join::rtt:
                std::cout << "session 1" << std::endl;
                psk->send(peerid, BlockchainMessage::Ping());
                s.expected_package_type = BlockchainMessage::Pong::rtt;
                break;
            case BlockchainMessage::Pong::rtt:
            {
                std::cout << "session 2" << std::endl;
                BlockchainMessage::Pong msg;
                std::move(package).get(msg);

                auto diff = system_clock::from_time_t(msg.stamp.tm) - system_clock::now();
                string message = msg.nodeid + ::beltpp::gm_time_t_to_gm_string(msg.stamp.tm);

                if ((seconds(-30) <= diff && diff < seconds(30)) &&
                    meshpp::verify_signature(msg.nodeid, message, msg.signature))
                {
                    ip_address addr = s.value;
                    addr.local = ip_destination();
                    names_verified[s.key] = std::make_pair(addr, ::std::chrono::steady_clock::now());
                }
                communications.erase(it);
                psk->send(peerid, beltpp::isocket_drop());
                break;
            }
            default:
                assert(false);
                break;
            }
        }

        return true;
    }
    unordered_map<string, session> communications;
    unordered_map<string, ip_address> names_not_verified;
    unordered_map<string, pair<ip_address, time_point>> names_verified;
};
}

