#pragma once

#include "message.hpp"

#include <belt.pp/socket.hpp>
#include <mesh.pp/p2psocket.hpp>
#include <mesh.pp/sessionutility.hpp>

namespace publiqpp
{
namespace detail
{
class node_internals;
}
class nodeid_service;

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
                              nodeid_service& service,
                              std::string const& nodeid,
                              beltpp::ip_address const& address);
    ~session_action_signatures() override;

    void initiate() override;
    bool process(beltpp::packet&& package) override;

    void erase(bool success, bool verified);

    beltpp::socket* psk;
    nodeid_service* pnodeid_service;
    std::string nodeid;
    beltpp::ip_address address;
};

class session_action_broadcast_address_info : public meshpp::session_action
{
public:
    session_action_broadcast_address_info(detail::node_internals* pimpl,
                                          meshpp::p2psocket::peer_id const& source_peer,
                                          BlockchainMessage::Broadcast&& msg);
    ~session_action_broadcast_address_info() override;

    void initiate() override;
    bool process(beltpp::packet&& package) override;

    detail::node_internals* pimpl;
    meshpp::p2psocket::peer_id source_peer;
    BlockchainMessage::Broadcast msg;
};

class session_action_storagefile : public meshpp::session_action
{
public:
    session_action_storagefile(beltpp::socket& sk,
                               std::string const& _file_uri);
    ~session_action_storagefile() override;

    void initiate() override;
    bool process(beltpp::packet&& package) override;

    std::string file_uri;
    beltpp::socket* psk;
};

}

