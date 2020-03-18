#pragma once

#include "message.hpp"

#include <belt.pp/socket.hpp>
#include <mesh.pp/p2psocket.hpp>
#include <mesh.pp/sessionutility.hpp>

#include <vector>
#include <string>
#include <functional>
#include <unordered_set>

namespace publiqpp
{
namespace detail
{
class node_internals;
}
class nodeid_service;

class session_action_connections : public meshpp::session_action<meshpp::nodeid_session_header>
{
public:
    session_action_connections(beltpp::socket& sk);
    ~session_action_connections() override;

    void initiate(meshpp::nodeid_session_header& header) override;
    bool process(beltpp::packet&& package, meshpp::nodeid_session_header& header) override;
    bool permanent() const override;

    beltpp::socket* psk;
    std::string peerid_to_drop;
};

class session_action_p2pconnections : public meshpp::session_action<meshpp::nodeid_session_header>
{
public:
    session_action_p2pconnections(meshpp::p2psocket& sk);
    ~session_action_p2pconnections() override;

    void initiate(meshpp::nodeid_session_header& header) override;
    bool process(beltpp::packet&& package, meshpp::nodeid_session_header& header) override;
    bool permanent() const override;

    meshpp::p2psocket* psk;
};

class session_action_signatures : public meshpp::session_action<meshpp::nodeid_session_header>
{
public:
    session_action_signatures(beltpp::socket& sk,
                              nodeid_service& service);
    ~session_action_signatures() override;

    void initiate(meshpp::nodeid_session_header& header) override;
    bool process(beltpp::packet&& package, meshpp::nodeid_session_header& header) override;
    bool permanent() const override;

    beltpp::socket* psk;
    nodeid_service* pnodeid_service;
    std::string nodeid;
    beltpp::ip_address address;
};

class session_action_broadcast_address_info : public meshpp::session_action<meshpp::nodeid_session_header>
{
public:
    session_action_broadcast_address_info(detail::node_internals& impl,
                                          meshpp::p2psocket::peer_id const& source_peer,
                                          BlockchainMessage::Broadcast&& msg);
    ~session_action_broadcast_address_info() override;

    void initiate(meshpp::nodeid_session_header& header) override;
    bool process(beltpp::packet&& package, meshpp::nodeid_session_header& header) override;
    bool permanent() const override;

    detail::node_internals* pimpl;
    meshpp::p2psocket::peer_id source_peer;
    BlockchainMessage::Broadcast msg;
};

class session_action_sync_request : public meshpp::session_action<meshpp::nodeid_session_header>
{
public:
    session_action_sync_request(detail::node_internals& impl,
                                beltpp::isocket& sk);
    ~session_action_sync_request() override;

    void initiate(meshpp::nodeid_session_header& header) override;
    bool process(beltpp::packet&& package, meshpp::nodeid_session_header& header) override;
    bool permanent() const override;

    detail::node_internals* pimpl;
    beltpp::isocket* psk;
    std::string current_peerid;
};

class session_action_header: public meshpp::session_action<meshpp::nodeid_session_header>
{
public:
    session_action_header(detail::node_internals& impl,
                          BlockchainMessage::BlockHeaderExtended const& promised_header);
    ~session_action_header() override;

    void initiate(meshpp::nodeid_session_header& header) override;
    bool process(beltpp::packet&& package, meshpp::nodeid_session_header& header) override;
    bool permanent() const override;

    static
    void process_request(beltpp::isocket::peer_id const& peerid,
                         BlockchainMessage::BlockHeaderRequest const& header_request,
                         publiqpp::detail::node_internals& impl);

    void process_response(meshpp::nodeid_session_header& header,
                          BlockchainMessage::BlockHeaderResponse&& header_response);

    void set_errored(std::string const& message, bool throw_for_debugging_only);

    //  this has opposite bool logic - true means error :)
    bool check_headers_vector(std::vector<BlockchainMessage::BlockHeaderExtended> const& header_vector);

    detail::node_internals* pimpl;
    uint64_t block_index_from;
    uint64_t block_index_to;
    BlockchainMessage::BlockHeaderExtended const promised_header;
    std::string current_peerid;
    std::vector<BlockchainMessage::BlockHeaderExtended> sync_headers;

protected:
    void _initiate(meshpp::nodeid_session_header& header, bool first);
};

class session_action_block : public meshpp::session_action<meshpp::nodeid_session_header>
{
public:
    class reason
    {
    public:
        enum e_v {safe_better, safe_revert, unsafe_better, unsafe_best};
        size_t poll_participants = 0;
        size_t poll_participants_with_stake = 0;
        double revert_coefficient = 0;
        e_v v = safe_better;
    };
    session_action_block(detail::node_internals& impl, reason e_reason);
    ~session_action_block() override;

    void initiate(meshpp::nodeid_session_header& header) override;
    bool process(beltpp::packet&& package, meshpp::nodeid_session_header& header) override;
    bool permanent() const override;

    static
    void process_request(beltpp::isocket::peer_id const& peerid,
                         BlockchainMessage::BlockchainRequest const& blockchain_request,
                         publiqpp::detail::node_internals& impl);

    void process_response(meshpp::nodeid_session_header& header,
                          BlockchainMessage::BlockchainResponse&& blockchain_response);

    void set_errored(std::string const& message, bool throw_for_debugging_only);

    detail::node_internals* pimpl;
    std::vector<BlockchainMessage::SignedBlock> sync_blocks;
    std::vector<BlockchainMessage::BlockHeaderExtended> sync_headers;
    reason m_reason;
};

class session_action_request_file : public meshpp::session_action<meshpp::nodeid_session_header>
{
public:
    session_action_request_file(std::string const& file_uri,
                                std::string const& nodeid,
                                detail::node_internals& impl);
    ~session_action_request_file() override;

    void initiate(meshpp::nodeid_session_header& header) override;
    bool process(beltpp::packet&& package, meshpp::nodeid_session_header& header) override;
    bool permanent() const override;

    bool need_to_revert_initiate;
    detail::node_internals* pimpl;
    std::string const file_uri;
    std::string const nodeid;
};

class session_action_save_file : public meshpp::session_action<meshpp::session_header>
{
public:
    session_action_save_file(detail::node_internals& impl,
                             BlockchainMessage::StorageFile&& file,
                             std::function<void(beltpp::packet&&)> const& callback);
    ~session_action_save_file() override;

    void initiate(meshpp::session_header& header) override;
    bool process(beltpp::packet&& package, meshpp::session_header& header) override;
    bool permanent() const override;

    detail::node_internals* pimpl;
    BlockchainMessage::StorageFile file;
    std::function<void(beltpp::packet&&)> callback;
};

class session_action_delete_file : public meshpp::session_action<meshpp::session_header>
{
public:
    session_action_delete_file(detail::node_internals& impl,
                               std::string const& uri,
                               std::function<void(beltpp::packet&&)> const& callback);
    ~session_action_delete_file() override;

    void initiate(meshpp::session_header& header) override;
    bool process(beltpp::packet&& package, meshpp::session_header& header) override;
    bool permanent() const override;

    detail::node_internals* pimpl;
    std::string uri;
    std::function<void(beltpp::packet&&)> callback;
};

class session_action_get_file_uris : public meshpp::session_action<meshpp::session_header>
{
public:
    session_action_get_file_uris(detail::node_internals& impl,
                                 std::function<void(beltpp::packet&&)> const& callback);
    ~session_action_get_file_uris() override;

    void initiate(meshpp::session_header& header) override;
    bool process(beltpp::packet&& package, meshpp::session_header& header) override;
    bool permanent() const override;

    detail::node_internals* pimpl;
    std::function<void(beltpp::packet&&)> callback;
};

class session_action_broadcast : public meshpp::session_action<meshpp::session_header>
{
public:
    session_action_broadcast(detail::node_internals& impl, BlockchainMessage::Broadcast& msg);
    ~session_action_broadcast() override;

    void initiate(meshpp::session_header& header) override;
    bool process(beltpp::packet&& package, meshpp::session_header& header) override;
    bool permanent() const override;

    detail::node_internals* pimpl;
    BlockchainMessage::Broadcast broadcast_msg;
};

}

