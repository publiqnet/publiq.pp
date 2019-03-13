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
    session_action_connections(beltpp::socket& sk);
    ~session_action_connections() override;

    void initiate(meshpp::session_header& header) override;
    bool process(beltpp::packet&& package, meshpp::session_header& header) override;
    bool permanent() const override;

    beltpp::socket* psk;
    std::string peerid_to_drop;
};

class session_action_p2pconnections : public meshpp::session_action
{
public:
    session_action_p2pconnections(meshpp::p2psocket& sk);
    ~session_action_p2pconnections() override;

    void initiate(meshpp::session_header& header) override;
    bool process(beltpp::packet&& package, meshpp::session_header& header) override;
    bool permanent() const override;

    meshpp::p2psocket* psk;
};

class session_action_signatures : public meshpp::session_action
{
public:
    session_action_signatures(beltpp::socket& sk,
                              nodeid_service& service);
    ~session_action_signatures() override;

    void initiate(meshpp::session_header& header) override;
    bool process(beltpp::packet&& package, meshpp::session_header& header) override;
    bool permanent() const override;

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

    void initiate(meshpp::session_header& header) override;
    bool process(beltpp::packet&& package, meshpp::session_header& header) override;
    bool permanent() const override;

    detail::node_internals* pimpl;
    meshpp::p2psocket::peer_id source_peer;
    BlockchainMessage::Broadcast msg;
};

class session_action_storagefile : public meshpp::session_action
{
public:
    session_action_storagefile(detail::node_internals* _pimpl,
                               std::string const& _file_uri);
    ~session_action_storagefile() override;

    void initiate(meshpp::session_header& header) override;
    bool process(beltpp::packet&& package, meshpp::session_header& header) override;
    bool permanent() const override;

    detail::node_internals* pimpl;
    std::string file_uri;
};

class session_action_sync_request: public meshpp::session_action
{
public:
    session_action_sync_request(detail::node_internals* _pimpl);
    ~session_action_sync_request() override;

    void initiate(meshpp::session_header& header) override;
    bool process(beltpp::packet&& package, meshpp::session_header& header) override;
    bool permanent() const override;

    detail::node_internals* pimpl;
    std::string current_peerid;
};

}

