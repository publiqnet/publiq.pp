#pragma once

#include <belt.pp/socket.hpp>
#include <mesh.pp/sessionutility.hpp>

namespace publiqpp
{

class session_action_connections : public meshpp::session_action
{
public:
    session_action_connections(beltpp::socket& sk,
                               beltpp::ip_address const& address);
    ~session_action_connections() override;

    void initiate() override;
    bool process(beltpp::packet&& package) override;

    bool need_to_drop;
    beltpp::socket* psk;
    beltpp::ip_address address;
};

class session_action_signatures : public meshpp::session_action
{
public:
    session_action_signatures(beltpp::socket& sk,
                              std::string const& nodeid);
    ~session_action_signatures() override;

    void initiate() override;
    bool process(beltpp::packet&& package) override;

    beltpp::socket* psk;
    std::string nodeid;
};

}

