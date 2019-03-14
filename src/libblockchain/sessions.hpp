#pragma once

#include "message.hpp"

#include <belt.pp/socket.hpp>
#include <mesh.pp/p2psocket.hpp>
#include <mesh.pp/sessionutility.hpp>

#include <vector>
#include <string>

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
    session_action_p2pconnections(meshpp::p2psocket& sk,
                                  detail::node_internals& impl);
    ~session_action_p2pconnections() override;

    void initiate(meshpp::session_header& header) override;
    bool process(beltpp::packet&& package, meshpp::session_header& header) override;
    bool permanent() const override;

    meshpp::p2psocket* psk;
    detail::node_internals* pimpl;
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
    session_action_broadcast_address_info(detail::node_internals& impl,
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
    session_action_storagefile(detail::node_internals& impl,
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
    session_action_sync_request(detail::node_internals& impl);
    ~session_action_sync_request() override;

    void initiate(meshpp::session_header& header) override;
    bool process(beltpp::packet&& package, meshpp::session_header& header) override;
    bool permanent() const override;

    detail::node_internals* pimpl;
    std::string current_peerid;
};

class session_action_header: public meshpp::session_action
{
public:
    session_action_header(detail::node_internals& impl,
                          uint64_t promised_block_number,
                          uint64_t promised_consensus_sum);
    ~session_action_header() override;

    void initiate(meshpp::session_header& header) override;
    bool process(beltpp::packet&& package, meshpp::session_header& header) override;
    bool permanent() const override;

    static
    void process_request(beltpp::isocket::peer_id const& peerid,
                         BlockchainMessage::BlockHeaderRequest2 const& header_request,
                         publiqpp::detail::node_internals& impl);

    void process_response(meshpp::session_header& header,
                          BlockchainMessage::BlockHeaderResponse2&& header_response);

    void set_errored(std::string const& message, bool throw_for_debugging_only);

    //  this has opposite bool logic - true means error :)
    bool check_headers_vector(std::vector<BlockchainMessage::BlockHeader> const& header_vector);

    detail::node_internals* pimpl;
    uint64_t block_index_from;
    uint64_t block_index_to;
    uint64_t const promised_block_number;
    uint64_t const promised_consensus_sum;
    std::vector<BlockchainMessage::BlockHeader> sync_headers;
};

}

