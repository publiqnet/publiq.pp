#include "sessions.hpp"
#include "common.hpp"
#include "message.hpp"
#include "communication_rpc.hpp"
#include "communication_p2p.hpp"
#include "node_internals.hpp"
#include "exception.hpp"

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
// --------------------------- session_action_connections ---------------------------
session_action_connections::session_action_connections(beltpp::socket& sk)
    : meshpp::session_action()
    , psk(&sk)
    , peerid_to_drop()
{}

session_action_connections::~session_action_connections()
{
    if (false == peerid_to_drop.empty())
        psk->send(peerid_to_drop, beltpp::isocket_drop());
}

void session_action_connections::initiate(meshpp::session_header& header)
{
    auto peerids = psk->open(header.address);
    if (peerids.size() != 1)
        throw std::logic_error("socket open by ip address must give just one peerid");

    header.peerid = peerids.front();
    expected_next_package_type = beltpp::isocket_join::rtt;
}

bool session_action_connections::process(beltpp::packet&& package, meshpp::session_header& header)
{
    bool code = true;

    if (expected_next_package_type == package.type() &&
        expected_next_package_type != size_t(-1))
    {
        switch (package.type())
        {
        case beltpp::isocket_join::rtt:
            std::cout << "session_action_connections - join" << std::endl;
            peerid_to_drop = header.peerid;
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
            peerid_to_drop.clear();
            std::cout << "action_connections - drop" << std::endl;
            break;
        case beltpp::isocket_protocol_error::rtt:
            errored = true;
            std::cout << "action_connections - protocol error" << std::endl;
            psk->send(header.peerid, beltpp::isocket_drop());
            peerid_to_drop.clear();
            break;
        case beltpp::isocket_open_error::rtt:
            std::cout << "action_connections - open error" << std::endl;
            errored = true;
            break;
        case beltpp::isocket_open_refused::rtt:
            std::cout << "action_connections - open refused" << std::endl;
            errored = true;
            break;
        default:
            code = false;
            break;
        }
    }

    return code;
}

bool session_action_connections::permanent() const
{
    return true;
}

// --------------------------- session_action_connections ---------------------------

session_action_p2pconnections::session_action_p2pconnections(meshpp::p2psocket& sk,
                                                             detail::node_internals& impl)
    : meshpp::session_action()
    , psk(&sk)
    , pimpl(&impl)
{}

session_action_p2pconnections::~session_action_p2pconnections()
{
}

void session_action_p2pconnections::initiate(meshpp::session_header& header)
{
    header.peerid = header.nodeid;
    expected_next_package_type = size_t(-1);
    completed = true;
}

bool session_action_p2pconnections::process(beltpp::packet&& package, meshpp::session_header& header)
{
    bool code = true;

    switch (package.type())
    {
    case beltpp::isocket_drop::rtt:
        errored = true;
        std::cout << "action_p2pconnections - drop" << std::endl;
        pimpl->remove_peer(header.peerid);
        break;
    case beltpp::isocket_protocol_error::rtt:
        errored = true;
        std::cout << "action_p2pconnections - protocol error" << std::endl;
        psk->send(header.peerid, beltpp::isocket_drop());
        pimpl->remove_peer(header.peerid);
        break;
    default:
        code = false;
        break;
    }

    return code;
}

bool session_action_p2pconnections::permanent() const
{
    return true;
}

// --------------------------- session_action_signatures ---------------------------

session_action_signatures::session_action_signatures(beltpp::socket& sk,
                                                     nodeid_service& service)
    : session_action()
    , psk(&sk)
    , pnodeid_service(&service)
    , address()
{}

session_action_signatures::~session_action_signatures()
{
    if (completed &&
        false == errored)
        erase(true, false);
    else if (expected_next_package_type != size_t(-1))
        erase(false, false);
}

void session_action_signatures::initiate(meshpp::session_header& header)
{
    psk->send(header.peerid, BlockchainMessage::Ping());
    expected_next_package_type = BlockchainMessage::Pong::rtt;

    nodeid = header.nodeid;
    address = header.address;
}

bool session_action_signatures::process(beltpp::packet&& package, meshpp::session_header& header)
{
    bool code = true;

    if (expected_next_package_type == package.type() &&
        expected_next_package_type != size_t(-1))
    {
        switch (package.type())
        {
        case BlockchainMessage::Pong::rtt:
        {
            //std::cout << "action_signatures -> pong received" << std::endl;
            BlockchainMessage::Pong msg;
            std::move(package).get(msg);

            auto diff = system_clock::from_time_t(msg.stamp.tm) - system_clock::now();
            string message = msg.node_address + ::beltpp::gm_time_t_to_gm_string(msg.stamp.tm);

            if ((chrono::seconds(-30) <= diff && diff < chrono::seconds(30)) &&
                meshpp::verify_signature(msg.node_address, message, msg.signature) &&
                header.nodeid == msg.node_address)
            {
                std::cout << "action_signatures - ok verify" << std::endl;

                erase(true, true);
            }
            else
            {
                errored = true;
                std::cout << "action_signatures -> signiture filed" << std::endl;

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

bool session_action_signatures::permanent() const
{
    return true;
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

// --------------------------- session_action_broadcast_address_info ---------------------------

session_action_broadcast_address_info::session_action_broadcast_address_info(detail::node_internals& impl,
                                                                             meshpp::p2psocket::peer_id const& _source_peer,
                                                                             BlockchainMessage::Broadcast&& _msg)
    : session_action()
    , pimpl(&impl)
    , source_peer(_source_peer)
    , msg(std::move(_msg))
{}

session_action_broadcast_address_info::~session_action_broadcast_address_info()
{}

void session_action_broadcast_address_info::initiate(meshpp::session_header&)
{
    std::cout << "action_broadcast - broadcasting" << std::endl;
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

bool session_action_broadcast_address_info::process(beltpp::packet&&, meshpp::session_header&)
{
    return false;
}

bool session_action_broadcast_address_info::permanent() const
{
    return false;
}

// --------------------------- session_action_storagefile ---------------------------

session_action_storagefile::session_action_storagefile(detail::node_internals& impl,
                                                       string const& _file_uri)
    : session_action()
    , pimpl(&impl)
    , file_uri(_file_uri)
{}

session_action_storagefile::~session_action_storagefile()
{}

void session_action_storagefile::initiate(meshpp::session_header& header)
{
    BlockchainMessage::GetStorageFile get_storagefile;
    get_storagefile.uri = file_uri;

    pimpl->m_ptr_rpc_socket->send(header.peerid, get_storagefile);
    expected_next_package_type = BlockchainMessage::StorageFile::rtt;
}

bool session_action_storagefile::process(beltpp::packet&& package, meshpp::session_header&)
{
    bool code = true;

    if (expected_next_package_type == package.type() &&
        expected_next_package_type != size_t(-1))
    {
        switch (package.type())
        {
        case BlockchainMessage::StorageFile::rtt:
        {
            std::cout << "action_storagefile -> File received" << std::endl;
            BlockchainMessage::StorageFile storage_file;
            package.get(storage_file);

            if (file_uri == meshpp::hash(storage_file.data))
            {
                std::cout << "action_storagefile -> File verified" << std::endl;

                if (!pimpl->m_slave_peer.empty())
                {
                    TaskRequest task_request;
                    task_request.task_id = ++pimpl->m_slave_taskid;
                    ::detail::assign_packet(task_request.package, package);
                    task_request.time_signed.tm = system_clock::to_time_t(system_clock::now());
                    meshpp::signature signed_msg = pimpl->m_pv_key.sign(std::to_string(task_request.task_id) +
                                                                        meshpp::hash(package.to_string()) +
                                                                        std::to_string(task_request.time_signed.tm));
                    task_request.signature = signed_msg.base58;

                    // send task to slave
                    pimpl->m_ptr_rpc_socket->send(pimpl->m_slave_peer, task_request);

                    pimpl->m_slave_tasks.add(task_request.task_id, package);
                }
                else
                    pimpl->reconnect_slave();
            }
            else
            {
                errored = true;
                std::cout << "action_storagefile -> File verification filed" << std::endl;
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

bool session_action_storagefile::permanent() const
{
    return false;
}

// --------------------------- session_action_sync_request ---------------------------

session_action_sync_request::session_action_sync_request(detail::node_internals& impl)
    : session_action()
    , pimpl(&impl)
{}

session_action_sync_request::~session_action_sync_request()
{
    if (false == current_peerid.empty())
        pimpl->all_sync_info.sync_responses.erase(current_peerid);
}

void session_action_sync_request::initiate(meshpp::session_header& header)
{
    pimpl->m_ptr_p2p_socket->send(header.peerid, BlockchainMessage::SyncRequest2());
    expected_next_package_type = BlockchainMessage::SyncResponse2::rtt;
}

bool session_action_sync_request::process(beltpp::packet&& package, meshpp::session_header& header)
{
    bool code = true;

    if (expected_next_package_type == package.type() &&
        expected_next_package_type != size_t(-1))
    {
        switch (package.type())
        {
        case SyncResponse2::rtt:
        {
            SyncResponse2 sync_response;
            std::move(package).get(sync_response);

            current_peerid = header.peerid;
            pimpl->all_sync_info.sync_responses[current_peerid] = std::move(sync_response);

            expected_next_package_type = size_t(-1);
            completed = true;

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

bool session_action_sync_request::permanent() const
{
    return true;
}

// --------------------------- session_action_header ---------------------------

session_action_header::session_action_header(detail::node_internals& impl,
                                             uint64_t _promised_block_number,
                                             uint64_t _promised_consensus_sum)
    : session_action()
    , pimpl(&impl)
    , block_index_from(_promised_block_number)
    , block_index_to(pimpl->m_blockchain.last_header().block_number)
    , promised_block_number(_promised_block_number)
    , promised_consensus_sum(_promised_consensus_sum)
{}

session_action_header::~session_action_header()
{
}

void session_action_header::initiate(meshpp::session_header& header)
{
    assert(false == header.peerid.empty());
    //  this assert means that the current session must have session_action_p2pconnections

    BlockHeaderRequest2 header_request;
    header_request.blocks_from = block_index_from;
    header_request.blocks_to = block_index_to;

    pimpl->m_ptr_p2p_socket->send(header.peerid, header_request);
    expected_next_package_type = BlockchainMessage::BlockHeaderResponse2::rtt;
}

bool session_action_header::process(beltpp::packet&& package, meshpp::session_header& header)
{
    bool code = true;

    if (expected_next_package_type == package.type() &&
        expected_next_package_type != size_t(-1))
    {
        switch (package.type())
        {
        case BlockHeaderResponse2::rtt:
        {
            BlockHeaderResponse2 header_response;
            std::move(package).get(header_response);

            process_response(header,
                             std::move(header_response));

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

bool session_action_header::permanent() const
{
    return true;
}

void session_action_header::process_request(beltpp::isocket::peer_id const& peerid,
                                            BlockchainMessage::BlockHeaderRequest2 const& header_request,
                                            publiqpp::detail::node_internals& impl)
{
    // headers always requested in reverse order!

    uint64_t from = impl.m_blockchain.length() - 1;
    from = from < header_request.blocks_from ? from : header_request.blocks_from;

    uint64_t to = header_request.blocks_to;
    to = to > from ? from : to;
    to = from > HEADER_TR_LENGTH && to < from - HEADER_TR_LENGTH ? from - HEADER_TR_LENGTH : to;

    BlockHeaderResponse2 header_response;
    for (auto index = from + 1; index > to; --index)
    {
        BlockHeader const& header = impl.m_blockchain.header_at(index - 1);

        header_response.block_headers.push_back(header);
    }
    //  header_response.block_headers has highest index - first element
    //                                and lowest index - last element

    impl.m_ptr_p2p_socket->send(peerid, header_response);
}

void session_action_header::process_response(meshpp::session_header& header,
                                             BlockchainMessage::BlockHeaderResponse2&& header_response)
{
    bool throw_for_debugging_only = false;

    //  validate received headers
    if (header_response.block_headers.empty())
        return set_errored("blockheader response. empty response received!", throw_for_debugging_only);

    sync_headers.insert(sync_headers.end(),
                        header_response.block_headers.begin(),
                        header_response.block_headers.end());

    //  sync_headers.front() has the highest index
    if (sync_headers.front().c_sum != promised_consensus_sum ||
        sync_headers.front().block_number != promised_block_number ||
        sync_headers.front().block_number != sync_headers.back().block_number + sync_headers.size() - 1 ||
        sync_headers.front().block_number < sync_headers.back().block_number)
        return set_errored("blockheader response. wrong data received!", throw_for_debugging_only);

    if (check_headers_vector(sync_headers))
        return set_errored("blockheader response. wrong data in response!", throw_for_debugging_only);

    // find last common header
    uint64_t lcb_index = 0;
    bool lcb_found = false;
    //  sync_headers.begin() has the highest index
    auto r_it = sync_headers.begin();
    for (;
         r_it != sync_headers.end() &&
         false == lcb_found;
         ++r_it)
    {
        string prev_hash;
        if (r_it->block_number == pimpl->m_blockchain.length())
            prev_hash = pimpl->m_blockchain.last_hash();
        else if (r_it->block_number < pimpl->m_blockchain.length())
        {
            BlockHeader const& header = pimpl->m_blockchain.header_at(r_it->block_number);
            prev_hash = header.prev_hash;
        }

        if (false == prev_hash.empty() &&
            prev_hash == r_it->prev_hash)
        {
            lcb_index = r_it->block_number - 1;
            lcb_found = true;
        }
    }

    if (lcb_found &&
        lcb_index == uint64_t(-1))
    {
        //  this guy is from a different blockchain network
        return set_errored("this guy is from a different blockchain network", throw_for_debugging_only);
    }
    else if (lcb_found)
    {
        //  r_it points to lcb block number or to sync_headers.end()
        if (r_it != sync_headers.end())
            sync_headers.erase(r_it, sync_headers.end());

        if (check_headers(sync_headers.back(), pimpl->m_blockchain.header_at(lcb_index)))
            return set_errored("blockheader response. header check failed!", throw_for_debugging_only);

        //  verify consensus_const
        vector<pair<uint64_t, uint64_t>> delta_vector;

        for (auto const& item : sync_headers)
            delta_vector.push_back(pair<uint64_t, uint64_t>(item.delta, item.c_const));

        uint64_t number = sync_headers.back().block_number - 1;
        assert(number == lcb_index);
        B_UNUSED(number);
        uint64_t delta_step = lcb_index < DELTA_STEP ? lcb_index : DELTA_STEP;

        for (uint64_t i = 0; i < delta_step; ++i)
        {
            BlockHeader const& tmp_header = pimpl->m_blockchain.header_at(lcb_index - i);

            delta_vector.push_back(pair<uint64_t, uint64_t>(tmp_header.delta, tmp_header.c_const));
        }

        for (auto it = delta_vector.begin(); it + delta_step != delta_vector.end(); ++it)
        {
            if (it->first > DELTA_UP)
            {
                size_t step = 0;
                uint64_t _delta = it->first;

                while (_delta > DELTA_UP && step < DELTA_STEP && it + step != delta_vector.end())
                {
                    ++step;
                    _delta = (it + step)->first;
                }

                if (step >= DELTA_STEP && it->second != (it + 1)->second * 2)
                    return set_errored("blockheader response. wrong consensus const up!", throw_for_debugging_only);
            }
            else if (it->first < DELTA_DOWN && it->second > 1)
            {
                size_t step = 0;
                uint64_t _delta = it->first;

                while (_delta < DELTA_DOWN && step < DELTA_STEP && it + step != delta_vector.end())
                {
                    ++step;
                    _delta = (it + step)->first;
                }

                if (step >= DELTA_STEP && it->second != (it + 1)->second / 2)
                    return set_errored("blockheader response. wrong consensus const down!", throw_for_debugging_only);
            }
        }

        /*BlockchainRequest2 blockchain_request;
        blockchain_request.blocks_from = sync_headers.back().block_number;
        blockchain_request.blocks_to = sync_headers.front().block_number;

        pimpl->m_ptr_p2p_socket->send(header.peerid, blockchain_request);*/

        completed = true;
        expected_next_package_type = size_t(-1);
        return;
    }
    else
    {
        block_index_from = sync_headers.back().block_number - 1;
        block_index_to = block_index_from > HEADER_TR_LENGTH ? block_index_from - HEADER_TR_LENGTH : 0;

        // request more headers
        initiate(header);
    }
}

void session_action_header::set_errored(string const& message, bool throw_for_debugging_only)
{
    if (throw_for_debugging_only)
        throw wrong_data_exception(message);
    errored = true;
}

//  this has opposite bool logic - true means error :)
bool session_action_header::check_headers_vector(std::vector<BlockchainMessage::BlockHeader> const& header_vector)
{
    bool t = false;
    auto it = header_vector.begin();
    for (++it; !t && it != header_vector.end(); ++it)
        t = check_headers(*(it - 1), *it);

    return t;
}
}

