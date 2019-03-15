#include "sessions.hpp"
#include "common.hpp"
#include "message.hpp"
#include "communication_rpc.hpp"
#include "communication_p2p.hpp"
#include "node_internals.hpp"
#include "exception.hpp"
#include "transaction_handler.hpp"

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

// --------------------------- session_action_p2pconnections ---------------------------

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
    pimpl->m_ptr_p2p_socket->send(header.peerid, BlockchainMessage::SyncRequest());
    expected_next_package_type = BlockchainMessage::SyncResponse::rtt;
}

bool session_action_sync_request::process(beltpp::packet&& package, meshpp::session_header& header)
{
    bool code = true;

    if (expected_next_package_type == package.type() &&
        expected_next_package_type != size_t(-1))
    {
        switch (package.type())
        {
        case SyncResponse::rtt:
        {
            SyncResponse sync_response;
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
{
    assert(false == pimpl->all_sync_info.blockchain_sync_in_progress);
    pimpl->all_sync_info.blockchain_sync_in_progress = true;
}

session_action_header::~session_action_header()
{
    if (false == current_peerid.empty())
        pimpl->all_sync_info.sync_headers.erase(current_peerid);

    pimpl->all_sync_info.blockchain_sync_in_progress = false;
}

void session_action_header::initiate(meshpp::session_header& header)
{
    assert(false == header.peerid.empty());
    //  this assert means that the current session must have session_action_p2pconnections

    BlockHeaderRequest header_request;
    header_request.blocks_from = block_index_from;
    header_request.blocks_to = block_index_to;

    pimpl->m_ptr_p2p_socket->send(header.peerid, header_request);
    expected_next_package_type = BlockchainMessage::BlockHeaderResponse::rtt;
}

bool session_action_header::process(beltpp::packet&& package, meshpp::session_header& header)
{
    bool code = true;

    if (expected_next_package_type == package.type() &&
        expected_next_package_type != size_t(-1))
    {
        switch (package.type())
        {
        case BlockHeaderResponse::rtt:
        {
            BlockHeaderResponse header_response;
            std::move(package).get(header_response);

            process_response(header, std::move(header_response));

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
                                            BlockchainMessage::BlockHeaderRequest const& header_request,
                                            publiqpp::detail::node_internals& impl)
{
    // headers always requested in reverse order!

    uint64_t from = impl.m_blockchain.length() - 1;
    from = from < header_request.blocks_from ? from : header_request.blocks_from;

    uint64_t to = header_request.blocks_to;
    to = to > from ? from : to;
    to = from > HEADER_TR_LENGTH && to < from - HEADER_TR_LENGTH ? from - HEADER_TR_LENGTH : to;

    BlockHeaderResponse header_response;
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
                                             BlockchainMessage::BlockHeaderResponse&& header_response)
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
    for (; r_it != sync_headers.end() && false == lcb_found; ++r_it)
    {
        string prev_hash;
        if (r_it->block_number == pimpl->m_blockchain.length())
            prev_hash = pimpl->m_blockchain.last_hash();
        else if (r_it->block_number < pimpl->m_blockchain.length())
        {
            BlockHeader const& _header = pimpl->m_blockchain.header_at(r_it->block_number);
            prev_hash = _header.prev_hash;
        }

        if (false == prev_hash.empty() && prev_hash == r_it->prev_hash)
        {
            lcb_index = r_it->block_number - 1;
            lcb_found = true;
        }
    }

    if (lcb_found && lcb_index == uint64_t(-1))
    {
        //  this guy is from a different blockchain network
        return set_errored("this guy is from a different blockchain network", throw_for_debugging_only);
    }
    else if (lcb_found)
    {
        //  r_it points to lcb block number or to sync_headers.end()
        if (r_it != sync_headers.end())
            sync_headers.erase(r_it, sync_headers.end());

        assert(sync_headers.back().block_number != 0);

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

        current_peerid = header.peerid;
        pimpl->all_sync_info.sync_headers[current_peerid] = std::move(sync_headers);
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

// --------------------------- session_action_block ---------------------------

session_action_block::session_action_block(detail::node_internals& impl)
    : session_action()
    , pimpl(&impl)
{}

session_action_block::~session_action_block()
{
}

void session_action_block::initiate(meshpp::session_header& header)
{
    assert(false == header.peerid.empty());
    //  this assert means that the current session must have session_action_p2pconnections

    sync_headers = std::move(pimpl->all_sync_info.sync_headers[header.peerid]);
    BlockchainRequest blockchain_request;
    blockchain_request.blocks_from = sync_headers.back().block_number;
    blockchain_request.blocks_to = sync_headers.front().block_number;

    pimpl->m_ptr_p2p_socket->send(header.peerid, blockchain_request);
    expected_next_package_type = BlockchainMessage::BlockchainResponse::rtt;
}

bool session_action_block::process(beltpp::packet&& package, meshpp::session_header& header)
{
    bool code = true;

    if (expected_next_package_type == package.type() &&
        expected_next_package_type != size_t(-1))
    {
        switch (package.type())
        {
        case BlockchainResponse::rtt:
        {
            BlockchainResponse blockchain_response;
            std::move(package).get(blockchain_response);

            bool have_signed_blocks = (false == blockchain_response.signed_blocks.empty());
            if (have_signed_blocks)
            {
                uint64_t temp_from = 0;
                uint64_t temp_to = 0;

                auto const& front = blockchain_response.signed_blocks.front().block_details;
                auto const& back = blockchain_response.signed_blocks.back().block_details;

                temp_from = front.header.block_number;
                temp_to = back.header.block_number;

                if(temp_from == temp_to)
                    //pimpl->writeln_node("processing block " + std::to_string(temp_from) +" from " + detail::peer_short_names(peerid));
                    pimpl->writeln_node("processing block " + std::to_string(temp_from));
                else
                    pimpl->writeln_node("proc. blocks [" + std::to_string(temp_from) +
                                        "," + std::to_string(temp_to) + "] from " + detail::peer_short_names(header.peerid));
            }

            process_response(header,
                             std::move(blockchain_response));
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

bool session_action_block::permanent() const
{
    return true;
}

void session_action_block::process_request(beltpp::isocket::peer_id const& peerid,
                                           BlockchainMessage::BlockchainRequest const& blockchain_request,
                                           publiqpp::detail::node_internals& impl)
{
    // blocks are always requested in regular order

    uint64_t number = impl.m_blockchain.length() - 1;
    uint64_t from = number < blockchain_request.blocks_from ? number : blockchain_request.blocks_from;

    uint64_t to = blockchain_request.blocks_to;
    to = to < from ? from : to;
    to = to > from + BLOCK_TR_LENGTH ? from + BLOCK_TR_LENGTH : to;
    to = to > number ? number : to;

    BlockchainResponse chain_response;
    for (auto i = from; i <= to; ++i)
    {
        SignedBlock const& signed_block = impl.m_blockchain.at(i);

        chain_response.signed_blocks.push_back(std::move(signed_block));
    }

    impl.m_ptr_p2p_socket->send(peerid, chain_response);
}

void session_action_block::process_response(meshpp::session_header& header,
                                            BlockchainMessage::BlockchainResponse&& blockchain_response)
{
    bool throw_for_debugging_only = false;

    //1. check received blockchain validity

    if (blockchain_response.signed_blocks.empty())
        return set_errored("blockchain response. empty response received!", throw_for_debugging_only);

    // find last common block
    uint64_t block_number = sync_headers.back().block_number;

    assert(block_number > 0);
    if (block_number == 0)
        throw std::logic_error("sync headers action must take care of this, the program will stop because of this");

    //2. check and add received blockchain to sync_blocks_vector for future process
    size_t length = sync_blocks.size();

    // put prev_signed_block in correct place
    SignedBlock const* prev_signed_block;
    if (sync_blocks.empty())
        prev_signed_block = &pimpl->m_blockchain.at(block_number - 1);
    else
        prev_signed_block = &(sync_blocks.back());

    auto header_it = sync_headers.rbegin() + length;

    if (header_it->prev_hash != meshpp::hash(prev_signed_block->block_details.to_string()))
        return set_errored("blockchain response. previous hash!", throw_for_debugging_only);

    ++header_it;
    for (auto& block_item : blockchain_response.signed_blocks)
    {
        Block& block = block_item.block_details;
        string str = block.to_string();

        // verify block signature
        if (!meshpp::verify_signature(meshpp::public_key(block_item.authority), str, block_item.signature))
            return set_errored("blockchain response. block signature!", throw_for_debugging_only);

        if (header_it != sync_headers.rend())
        {
            if (*(header_it - 1) != block.header || header_it->prev_hash != meshpp::hash(str))
                return set_errored("blockchain response. block header!", throw_for_debugging_only);

            ++header_it;
        }

        // verify block transactions
        for (auto tr_it = block.signed_transactions.begin(); tr_it != block.signed_transactions.end(); ++tr_it)
        {
            if (!meshpp::verify_signature(meshpp::public_key(tr_it->authority),
                                          tr_it->transaction_details.to_string(),
                                          tr_it->signature))
                return set_errored("blockchain response. transaction signature!", throw_for_debugging_only);

            action_validate(*pimpl, *tr_it);

            system_clock::time_point creation = system_clock::from_time_t(tr_it->transaction_details.creation.tm);
            system_clock::time_point expiry = system_clock::from_time_t(tr_it->transaction_details.expiry.tm);

            if (expiry - creation > chrono::hours(TRANSACTION_MAX_LIFETIME_HOURS))
                return set_errored("blockchain response. too long lifetime for transaction", throw_for_debugging_only);

            if (creation - chrono::seconds(NODES_TIME_SHIFT) > system_clock::from_time_t(block.header.time_signed.tm))
                return set_errored("blockchain response. transaction from the future!", throw_for_debugging_only);

            if (expiry < system_clock::from_time_t(block.header.time_signed.tm))
                return set_errored("blockchain response. expired transaction!", throw_for_debugging_only);
        }

        // store blocks for future use
        sync_blocks.push_back(std::move(block_item));
    }

    // request new chain if needed
    if (sync_blocks.size() < BLOCK_INSERT_LENGTH &&
        sync_blocks.size() < sync_headers.size())
    {
        BlockchainRequest blockchain_request;
        blockchain_request.blocks_from = (header_it - 1)->block_number;
        blockchain_request.blocks_to = sync_headers.begin()->block_number;

        pimpl->m_ptr_p2p_socket->send(header.peerid, blockchain_request);

        return; // will wait new chain
    }

    //3. all needed blocks received, start to check
    unordered_map<string, system_clock::time_point> transaction_cache_backup = pimpl->m_transaction_cache;

    auto now = system_clock::now();
    beltpp::on_failure guard([this, &transaction_cache_backup]
    {
        pimpl->discard();
        pimpl->m_transaction_cache = std::move(transaction_cache_backup);
    });

    uint64_t lcb_number = sync_headers.rbegin()->block_number - 1;
    vector<SignedTransaction> reverted_transactions;
    bool clear_pool = sync_blocks.size() < sync_headers.size();

    //  collect transactions to be reverted from blockchain
    //
    size_t blockchain_length = pimpl->m_blockchain.length();

    for (size_t index = lcb_number + 1; index < blockchain_length; ++index)
    {
        SignedBlock const& signed_block = pimpl->m_blockchain.at(index);

        reverted_transactions.insert(reverted_transactions.end(),
                                     signed_block.block_details.signed_transactions.begin(),
                                     signed_block.block_details.signed_transactions.end());
    }

    vector<SignedTransaction> pool_transactions;
    //  collect transactions to be reverted from pool
    //  revert transactions from pool
    revert_pool(system_clock::to_time_t(now - chrono::seconds(NODES_TIME_SHIFT)), *pimpl, pool_transactions);

    //  revert blocks
    //  calculate back to get state at LCB point
    for (size_t index = blockchain_length - 1;
         index < blockchain_length && index > lcb_number;
         --index)
    {
        SignedBlock const& signed_block = pimpl->m_blockchain.at(index);
        pimpl->m_blockchain.remove_last_block();
        pimpl->m_action_log.revert();

        Block const& block = signed_block.block_details;

        // decrease all reward amounts from balances and revert reward
        for (auto it = block.rewards.crbegin(); it != block.rewards.crend(); ++it)
            pimpl->m_state.decrease_balance(it->to, it->amount);

        // calculate back transactions
        for (auto it = block.signed_transactions.crbegin(); it != block.signed_transactions.crend(); ++it)
        {
            revert_transaction(*it, *pimpl, signed_block.authority);

            string key = meshpp::hash(it->to_string());
            pimpl->m_transaction_cache.erase(key);
        }
    }
    //  update the variable, just in case it will be needed down the code
    blockchain_length = pimpl->m_blockchain.length();

    unordered_set<string> set_tr_hashes_to_remove;

    // verify new received blocks
    BlockHeader const& prev_header = pimpl->m_blockchain.header_at(lcb_number);
    uint64_t c_const = prev_header.c_const;

    for (auto const& signed_block : sync_blocks)
    {
        Block const& block = signed_block.block_details;

        // verify consensus_delta
        Coin amount = pimpl->m_state.get_balance(signed_block.authority);
        uint64_t delta = pimpl->calc_delta(signed_block.authority, amount.whole, block.header.prev_hash, c_const);

        if (delta != block.header.delta)
            return set_errored("blockchain response. consensus delta!", throw_for_debugging_only);

        // verify miner balance at mining time
        if (coin(amount) < MINE_AMOUNT_THRESHOLD)
            return set_errored("blockchain response. miner balance!", throw_for_debugging_only);

        // verify block transactions
        for (auto const& tr_item : block.signed_transactions)
        {
            string key = meshpp::hash(tr_item.to_string());

            set_tr_hashes_to_remove.insert(key);

            if (pimpl->m_transaction_cache.find(key) != pimpl->m_transaction_cache.end())
                return set_errored("blockchain response. transaction double use!", throw_for_debugging_only);

            pimpl->m_transaction_cache[key] = system_clock::from_time_t(tr_item.transaction_details.creation.tm);

            if (!apply_transaction(tr_item, *pimpl, signed_block.authority))
                return set_errored("blockchain response. sender balance!", throw_for_debugging_only);
        }

        // verify block rewards
        if (check_rewards(block, signed_block.authority, *pimpl))
            return set_errored("blockchain response. block rewards!", throw_for_debugging_only);

        // increase all reward amounts to balances
        for (auto const& reward_item : block.rewards)
            pimpl->m_state.increase_balance(reward_item.to, reward_item.amount);

        // Insert to blockchain
        pimpl->m_blockchain.insert(signed_block);
        pimpl->m_action_log.log_block(signed_block);

        c_const = block.header.c_const;
    }

    if (false == clear_pool)
        reverted_transactions.insert(reverted_transactions.end(),
                                     pool_transactions.begin(),
                                     pool_transactions.end());

    // apply back the rest of the transaction pool
    //
    for (auto const& signed_transaction : reverted_transactions)
    {
        string key = meshpp::hash(signed_transaction.to_string());
        if (now - chrono::seconds(NODES_TIME_SHIFT) <=
            system_clock::from_time_t(signed_transaction.transaction_details.expiry.tm) &&
            0 == set_tr_hashes_to_remove.count(key))
        {
            if (apply_transaction(signed_transaction, *pimpl))
            {
                pimpl->m_action_log.log_transaction(signed_transaction);
                pimpl->m_transaction_pool.push_back(signed_transaction);
                pimpl->m_transaction_cache[key] =
                        system_clock::from_time_t(signed_transaction.transaction_details.creation.tm);
            }
        }
    }

    pimpl->save(guard);

    // request new chain if the process was stopped
    // by BLOCK_INSERT_LENGTH restriction
    length = sync_blocks.size();
    if (length < sync_headers.size())
    {
        // clear already inserted blocks and headers
        sync_blocks.clear();
        for (size_t i = 0; i < length; ++i)
            sync_headers.pop_back();

        BlockchainRequest blockchain_request;
        blockchain_request.blocks_from = sync_headers.back().block_number;
        blockchain_request.blocks_to = sync_headers.front().block_number;

        pimpl->m_ptr_p2p_socket->send(header.peerid, blockchain_request);
    }
    else
    {
        completed = true;
        expected_next_package_type = size_t(-1);

        if (pimpl->m_node_type == NodeType::channel)
            broadcast_storage_info(*pimpl);

        if (pimpl->m_node_type == NodeType::storage && !pimpl->m_slave_peer.empty())
        {
            StatInfo stat_info;
            TaskRequest task_request;
            task_request.task_id = ++pimpl->m_slave_taskid;
            ::detail::assign_packet(task_request.package, stat_info);
            task_request.time_signed.tm = system_clock::to_time_t(now);
            meshpp::signature signed_msg = pimpl->m_pv_key.sign(std::to_string(task_request.task_id) +
                                                                meshpp::hash(stat_info.to_string()) +
                                                                std::to_string(task_request.time_signed.tm));
            task_request.signature = signed_msg.base58;

            // send task to slave
            pimpl->m_ptr_rpc_socket->send(pimpl->m_slave_peer, task_request);

            beltpp::packet task_packet;
            task_packet.set(stat_info);

            pimpl->m_slave_tasks.add(task_request.task_id, task_packet);
        }
    }
}

void session_action_block::set_errored(string const& message, bool throw_for_debugging_only)
{
    if (throw_for_debugging_only)
        throw wrong_data_exception(message);
    errored = true;
}
}

