#include "sessions.hpp"
#include "common.hpp"
#include "message.hpp"
#include "communication_rpc.hpp"
#include "communication_p2p.hpp"
#include "node_internals.hpp"
#include "exception.hpp"
#include "transaction_handler.hpp"

#include <belt.pp/utility.hpp>
#include <belt.pp/scope_helper.hpp>

#include <mesh.pp/cryptoutility.hpp>

#include <algorithm>
#include <map>

namespace chrono = std::chrono;
using chrono::system_clock;
using chrono::steady_clock;
using std::string;
using std::multimap;

namespace publiqpp
{
// --------------------------- session_action_connections ---------------------------
session_action_connections::session_action_connections(beltpp::socket& sk)
    : meshpp::session_action<meshpp::nodeid_session_header>()
    , psk(&sk)
    , peerid_to_drop()
{}

session_action_connections::~session_action_connections()
{
    if (false == peerid_to_drop.empty())
        psk->send(peerid_to_drop, beltpp::packet(beltpp::isocket_drop()));
}

void session_action_connections::initiate(meshpp::nodeid_session_header& header)
{
    auto peerids = psk->open(header.address);
    if (peerids.size() != 1)
        throw std::logic_error("socket open by ip address must give just one peerid");

    header.peerid = peerids.front();
    expected_next_package_type = beltpp::isocket_join::rtt;
}

bool session_action_connections::process(beltpp::packet&& package, meshpp::nodeid_session_header& header)
{
    bool code = true;

    if (expected_next_package_type == package.type() &&
        expected_next_package_type != size_t(-1))
    {
        switch (package.type())
        {
        case beltpp::isocket_join::rtt:
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
            break;
        case beltpp::isocket_protocol_error::rtt:
            errored = true;
            peerid_to_drop.clear();
            break;
        case beltpp::isocket_open_error::rtt:
            errored = true;
            break;
        case beltpp::isocket_open_refused::rtt:
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
    : meshpp::session_action<meshpp::nodeid_session_header>()
    , psk(&sk)
    , pimpl(&impl)
{}

session_action_p2pconnections::~session_action_p2pconnections()
{}

void session_action_p2pconnections::initiate(meshpp::nodeid_session_header& header)
{
    header.peerid = header.nodeid;
    expected_next_package_type = size_t(-1);
    completed = true;
}

bool session_action_p2pconnections::process(beltpp::packet&& package, meshpp::nodeid_session_header&)
{
    bool code = false;

    switch (package.type())
    {
    case beltpp::isocket_drop::rtt:
        errored = true;
        break;
    case beltpp::isocket_protocol_error::rtt:
        errored = true;
        break;
    default:
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
    : session_action<meshpp::nodeid_session_header>()
    , psk(&sk)
    , pnodeid_service(&service)
    , address()
{}

session_action_signatures::~session_action_signatures()
{
    if (completed &&
        false == errored)
        pnodeid_service->keep_successful(nodeid, address, false);
    else if (expected_next_package_type != size_t(-1))
        pnodeid_service->erase_failed(nodeid, address);
}

void session_action_signatures::initiate(meshpp::nodeid_session_header& header)
{
    psk->send(header.peerid, beltpp::packet(BlockchainMessage::Ping()));
    expected_next_package_type = BlockchainMessage::Pong::rtt;

    nodeid = header.nodeid;
    address = header.address;
}

bool session_action_signatures::process(beltpp::packet&& package, meshpp::nodeid_session_header& header)
{
    bool code = true;

    beltpp::on_failure guard([this]{ errored = true; });

    if (expected_next_package_type == package.type() &&
        expected_next_package_type != size_t(-1))
    {
        switch (package.type())
        {
        case BlockchainMessage::Pong::rtt:
        {
            BlockchainMessage::Pong msg;
            std::move(package).get(msg);

            auto diff = system_clock::from_time_t(msg.stamp.tm) - system_clock::now();
            string message = msg.node_address + ::beltpp::gm_time_t_to_gm_string(msg.stamp.tm);

            if ((chrono::seconds(-30) <= diff && diff < chrono::seconds(30)) &&
                meshpp::verify_signature(msg.node_address, message, msg.signature) &&
                header.nodeid == msg.node_address)
            {
                pnodeid_service->keep_successful(nodeid, address, true);
            }
            else
            {
                errored = true;

                pnodeid_service->erase_failed(nodeid, address);
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

    guard.dismiss();

    return code;
}

bool session_action_signatures::permanent() const
{
    return true;
}

// --------------------------- session_action_broadcast_address_info ---------------------------

session_action_broadcast_address_info::session_action_broadcast_address_info(detail::node_internals& impl,
                                                                             meshpp::p2psocket::peer_id const& _source_peer,
                                                                             BlockchainMessage::Broadcast&& _msg)
    : session_action<meshpp::nodeid_session_header>()
    , pimpl(&impl)
    , source_peer(_source_peer)
    , msg(std::move(_msg))
{}

session_action_broadcast_address_info::~session_action_broadcast_address_info()
{}

void session_action_broadcast_address_info::initiate(meshpp::nodeid_session_header&)
{
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

bool session_action_broadcast_address_info::process(beltpp::packet&&, meshpp::nodeid_session_header&)
{
    return false;
}

bool session_action_broadcast_address_info::permanent() const
{
    return false;
}

// --------------------------- session_action_sync_request ---------------------------

session_action_sync_request::session_action_sync_request(detail::node_internals& impl)
    : session_action<meshpp::nodeid_session_header>()
    , pimpl(&impl)
{}

session_action_sync_request::~session_action_sync_request()
{
    if (errored)
        pimpl->all_sync_info.sync_responses.erase(current_peerid);
}

void session_action_sync_request::initiate(meshpp::nodeid_session_header& header)
{
    pimpl->m_ptr_p2p_socket->send(header.peerid, beltpp::packet(BlockchainMessage::SyncRequest()));
    expected_next_package_type = BlockchainMessage::SyncResponse::rtt;
}

bool session_action_sync_request::process(beltpp::packet&& package, meshpp::nodeid_session_header& header)
{
    bool code = true;

    beltpp::on_failure guard([this]{ errored = true; });

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

    guard.dismiss();

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
    : session_action<meshpp::nodeid_session_header>()
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
    if (false == current_peerid.empty() &&
        false == errored)
        pimpl->all_sync_info.sync_headers.erase(current_peerid);

    pimpl->all_sync_info.blockchain_sync_in_progress = false;
}

void session_action_header::initiate(meshpp::nodeid_session_header& header)
{
    assert(false == header.peerid.empty());
    //  this assert means that the current session must have session_action_p2pconnections

    BlockHeaderRequest header_request;
    header_request.blocks_from = block_index_from;
    header_request.blocks_to = block_index_to;

    pimpl->m_ptr_p2p_socket->send(header.peerid, beltpp::packet(header_request));
    expected_next_package_type = BlockchainMessage::BlockHeaderResponse::rtt;
}

bool session_action_header::process(beltpp::packet&& package, meshpp::nodeid_session_header& header)
{
    bool code = true;

    beltpp::on_failure guard([this]{ errored = true; });

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

    guard.dismiss();

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
        BlockHeaderExtended const& header = impl.m_blockchain.header_ex_at(index - 1);

        header_response.block_headers.push_back(header);
    }
    //  header_response.block_headers has highest index - first element
    //                                and lowest index - last element

    impl.m_ptr_p2p_socket->send(peerid, beltpp::packet(header_response));
}

void session_action_header::process_response(meshpp::nodeid_session_header& header,
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

    if(system_clock::from_time_t(sync_headers.front().time_signed.tm) > system_clock::now() + chrono::seconds(NODES_TIME_SHIFT))
        return set_errored("blockheader response. block from future received!", throw_for_debugging_only);

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

        if (check_headers(sync_headers.back(), pimpl->m_blockchain.header_ex_at(lcb_index)))
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
bool session_action_header::check_headers_vector(std::vector<BlockchainMessage::BlockHeaderExtended> const& header_vector)
{
    bool t = false;
    auto it = header_vector.begin();
    for (++it; !t && it != header_vector.end(); ++it)
        t = check_headers(*(it - 1), *it);

    return t;
}

// --------------------------- session_action_block ---------------------------

session_action_block::session_action_block(detail::node_internals& impl)
    : session_action<meshpp::nodeid_session_header>()
    , pimpl(&impl)
{}

session_action_block::~session_action_block()
{}

void session_action_block::initiate(meshpp::nodeid_session_header& header)
{
    assert(false == header.peerid.empty());
    //  this assert means that the current session must have session_action_p2pconnections

    sync_headers = std::move(pimpl->all_sync_info.sync_headers[header.peerid]);
    BlockchainRequest blockchain_request;
    blockchain_request.blocks_from = sync_headers.back().block_number;
    blockchain_request.blocks_to = sync_headers.front().block_number;

    pimpl->m_ptr_p2p_socket->send(header.peerid, beltpp::packet(blockchain_request));
    expected_next_package_type = BlockchainMessage::BlockchainResponse::rtt;
}

bool session_action_block::process(beltpp::packet&& package, meshpp::nodeid_session_header& header)
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
                    pimpl->writeln_node("validating block " + std::to_string(temp_from));
                else
                    pimpl->writeln_node("validating blocks [" + std::to_string(temp_from) +
                                        "," + std::to_string(temp_to) + "] from " + detail::peer_short_names(header.peerid));
            }

            process_response(header, std::move(blockchain_response));
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

    impl.m_ptr_p2p_socket->send(peerid, beltpp::packet(chain_response));
}

void session_action_block::process_response(meshpp::nodeid_session_header& header,
                                            BlockchainMessage::BlockchainResponse&& blockchain_response)
{
    bool throw_for_debugging_only = false;

    //1. check received blockchain validity

    if (blockchain_response.signed_blocks.empty())
        return set_errored("blockchain response. empty response received!", throw_for_debugging_only);

    if (blockchain_response.signed_blocks.size() + sync_blocks.size() >
        sync_headers.size())
        return set_errored("blockchain response. more than expected blocks received", throw_for_debugging_only);

    // find last common block
    uint64_t block_number = sync_headers.back().block_number;

    assert(block_number > 0);
    assert(block_number <= pimpl->m_blockchain.length());
    if (block_number == 0)
        throw std::logic_error("sync headers action must take care of this, the program will stop because of this");

    //2. check and add received blockchain to sync_blocks_vector for future process
    string prev_block_hash;
    if (sync_blocks.empty())
    {
        if (block_number == pimpl->m_blockchain.length())
            prev_block_hash = pimpl->m_blockchain.last_hash();
        else
            prev_block_hash = pimpl->m_blockchain.header_at(block_number).prev_hash;
    }
    else
        prev_block_hash = meshpp::hash(sync_blocks.back().block_details.to_string());

    assert(sync_blocks.size() < sync_headers.size());
    auto header_it = sync_headers.rbegin() + sync_blocks.size();

    if (header_it->prev_hash != prev_block_hash)
        return set_errored("blockchain response. previous hash!", throw_for_debugging_only);

    for (auto& block_item : blockchain_response.signed_blocks)
    {
        Block& block = block_item.block_details;
        string str = block.to_string();

        if(block.signed_transactions.size() > BLOCK_MAX_TRANSACTIONS)
            return set_errored("blockchain response. block max transactions count!", throw_for_debugging_only);

        // verify block signature
        if (!meshpp::verify_signature(meshpp::public_key(block_item.authorization.address), str, block_item.authorization.signature))
            return set_errored("blockchain response. block signature!", throw_for_debugging_only);

        BlockHeaderExtended& temp_header_ex = *header_it;
        BlockHeader temp_header;
        temp_header = temp_header_ex;
        if (temp_header != block.header || temp_header_ex.block_hash != meshpp::hash(str))
            return set_errored("blockchain response. block header!", throw_for_debugging_only);

        ++header_it;

        // verify block transactions
        for (auto tr_it = block.signed_transactions.begin(); tr_it != block.signed_transactions.end(); ++tr_it)
        {
            signed_transaction_validate(*tr_it, system_clock::from_time_t(block.header.time_signed.tm), std::chrono::seconds(0));

            action_validate(*pimpl, *tr_it, true);
        }

        // store blocks for future use
        sync_blocks.push_back(std::move(block_item));
    }

    // request new chain if needed
    if (sync_blocks.size() < BLOCK_INSERT_LENGTH &&
        sync_blocks.size() < sync_headers.size())
    {
        BlockchainRequest blockchain_request;
        blockchain_request.blocks_from = header_it->block_number;
        blockchain_request.blocks_to = sync_headers.begin()->block_number;

        pimpl->m_ptr_p2p_socket->send(header.peerid, beltpp::packet(blockchain_request));

        return; // will wait for new chain
    }

    pimpl->writeln_node("inserting " + std::to_string(sync_blocks.size()) + " validated blocks");

    //3. all needed blocks received, start to check
    pimpl->m_transaction_cache.backup();

    auto now = system_clock::now();
    beltpp::on_failure guard([this]
    {
        pimpl->discard();
        pimpl->m_transaction_cache.restore();
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

    multimap<BlockchainMessage::ctime, SignedTransaction> pool_transactions;
    //  collect transactions to be reverted from pool
    //  revert transactions from pool
    revert_pool(system_clock::to_time_t(now), *pimpl, pool_transactions);

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
            pimpl->m_state.decrease_balance(it->to, it->amount, state_layer::chain);

        // calculate back transactions
        for (auto it = block.signed_transactions.crbegin(); it != block.signed_transactions.crend(); ++it)
        {
            revert_transaction(*it, *pimpl, signed_block.authorization.address);
            pimpl->m_transaction_cache.erase_chain(*it);
        }
    }
    //  update the variable, just in case it will be needed down the code
    blockchain_length = pimpl->m_blockchain.length();

    // verify new received blocks
    BlockHeader const& prev_header = pimpl->m_blockchain.header_at(lcb_number);
    uint64_t c_const = prev_header.c_const;

    for (auto const& signed_block : sync_blocks)
    {
        Block const& block = signed_block.block_details;

        // verify consensus_delta
        Coin amount = pimpl->m_state.get_balance(signed_block.authorization.address, state_layer::pool);
        uint64_t delta = pimpl->calc_delta(signed_block.authorization.address, amount.whole, block.header.prev_hash, c_const);

        if (delta != block.header.delta)
            return set_errored("blockchain response. consensus delta!", throw_for_debugging_only);

        // verify miner balance at mining time
        if (coin(amount) < MINE_AMOUNT_THRESHOLD)
            return set_errored("blockchain response. miner balance!", throw_for_debugging_only);

        // verify block transactions
        time_t prev_transaction_time = 0;
        for (auto const& tr_item : block.signed_transactions)
        {
            if (false == pimpl->m_transaction_cache.add_chain(tr_item))
                return set_errored("blockchain response. transaction double use!", throw_for_debugging_only);

            if (!apply_transaction(tr_item, *pimpl, signed_block.authorization.address))
                return set_errored("blockchain response. apply_transaction().", throw_for_debugging_only);

            if (prev_transaction_time > tr_item.transaction_details.creation.tm)
                return set_errored("blockchain response. transaction time sorting!", throw_for_debugging_only);

            prev_transaction_time = tr_item.transaction_details.creation.tm;
        }

        // verify block rewards
        if (check_rewards(block, signed_block.authorization.address, *pimpl))
            return set_errored("blockchain response. block rewards!", throw_for_debugging_only);

        // increase all reward amounts to balances
        for (auto const& reward_item : block.rewards)
            pimpl->m_state.increase_balance(reward_item.to, reward_item.amount, state_layer::chain);

        // Insert to blockchain
        pimpl->m_blockchain.insert(signed_block);
        pimpl->m_action_log.log_block(signed_block);

        c_const = block.header.c_const;
    }

    size_t chain_reverted_count = reverted_transactions.size();
    if (false == clear_pool)
    {
        for(auto&& item : pool_transactions)
            reverted_transactions.push_back(std::move(item.second));
    }

    // apply back the rest of the transaction pool
    //
    for (size_t index = 0; index != reverted_transactions.size(); ++index)
    {
        auto& signed_transaction = reverted_transactions[index];

        bool complete = false;
        if (index < chain_reverted_count)
            complete = true;
        else
        {
            auto code = action_authorization_process(*pimpl, signed_transaction);
            if (code.complete)
                complete = true;
        }

        if (now - chrono::seconds(NODES_TIME_SHIFT) <=
            system_clock::from_time_t(signed_transaction.transaction_details.expiry.tm) &&
            false == pimpl->m_transaction_cache.contains(signed_transaction))
        {
            bool ok_logic = true;
            if (complete ||
                false == action_can_apply(*pimpl, signed_transaction.transaction_details.action))
            {
                ok_logic = apply_transaction(signed_transaction, *pimpl);
                if (ok_logic)
                    pimpl->m_action_log.log_transaction(signed_transaction);
            }

            if (ok_logic)
            {
                pimpl->m_transaction_pool.push_back(signed_transaction);
                pimpl->m_transaction_cache.add_pool(signed_transaction, complete);
            }
        }
    }

    pimpl->save(guard);

    // request new chain if the process was stopped
    // by BLOCK_INSERT_LENGTH restriction
    if (sync_blocks.size() < sync_headers.size())
    {
        // clear already inserted blocks and headers
        sync_headers.resize(sync_headers.size() - sync_blocks.size());
        sync_blocks.clear();

        BlockchainRequest blockchain_request;
        blockchain_request.blocks_from = sync_headers.back().block_number;
        blockchain_request.blocks_to = sync_headers.front().block_number;

        pimpl->m_ptr_p2p_socket->send(header.peerid, beltpp::packet(blockchain_request));
    }
    else
    {
        completed = true;
        expected_next_package_type = size_t(-1);

        /*if (pimpl->m_node_type == NodeType::channel)
            broadcast_storage_info(*pimpl);


        if (pimpl->m_node_type == NodeType::storage && !pimpl->m_slave_peer.empty())
        {
            ServiceStatistics service_statistics;
            TaskRequest task_request;
            task_request.task_id = ++pimpl->m_slave_taskid;
            ::detail::assign_packet(task_request.package, service_statistics);
            task_request.time_signed.tm = system_clock::to_time_t(now);
            meshpp::signature signed_msg = pimpl->m_pv_key.sign(std::to_string(task_request.task_id) +
                                                                meshpp::hash(service_statistics.to_string()) +
                                                                std::to_string(task_request.time_signed.tm));
            task_request.signature = signed_msg.base58;

            // send task to slave
            pimpl->m_ptr_rpc_socket->send(pimpl->m_slave_peer, task_request);

            beltpp::packet task_packet;
            task_packet.set(service_statistics);

            pimpl->m_slave_tasks.add(task_request.task_id, task_packet);
        }
        */
    }
}

void session_action_block::set_errored(string const& message, bool throw_for_debugging_only)
{
    if (throw_for_debugging_only)
        throw wrong_data_exception(message);
    errored = true;
}

// --------------------------- session_action_save_file ---------------------------

session_action_save_file::session_action_save_file(detail::node_internals& impl,
                                                   StorageFile&& _file,
                                                   beltpp::isocket& sk,
                                                   beltpp::isocket::peer_id const& _peerid)
    : meshpp::session_action<meshpp::session_header>()
    , pimpl(&impl)
    , file(_file)
    , psk(&sk)
    , peerid(_peerid)
{}

session_action_save_file::~session_action_save_file()
{
    if (size_t(-1) != expected_next_package_type ||
        errored)
    {
        BlockchainMessage::RemoteError msg;
        msg.message = "unknown error uploading the file";
        psk->send(peerid, beltpp::packet(std::move(msg)));
    }
}

void session_action_save_file::initiate(meshpp::session_header&/* header*/)
{
    pimpl->m_slave_node->send(beltpp::packet(std::move(file)));
    pimpl->m_slave_node->wake();
    expected_next_package_type = BlockchainMessage::StorageFileAddress::rtt;
}

bool session_action_save_file::process(beltpp::packet&& package, meshpp::session_header&/* header*/)
{
    bool code = true;

    beltpp::on_failure guard([this]{ errored = true; });

    if (expected_next_package_type == package.type() &&
        expected_next_package_type != size_t(-1))
    {
        switch (package.type())
        {
        case BlockchainMessage::StorageFileAddress::rtt:
        {
            BlockchainMessage::StorageFileAddress msg;
            std::move(package).get(msg);

            psk->send(peerid, beltpp::packet(std::move(msg)));

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
        psk->send(peerid, std::move(package));
        completed = true;
        expected_next_package_type = size_t(-1);
    }

    guard.dismiss();

    return code;
}

bool session_action_save_file::permanent() const
{
    return false;
}

// --------------------------- session_action_delete_file ---------------------------

session_action_delete_file::session_action_delete_file(detail::node_internals& impl,
                                                       string const& _uri,
                                                       beltpp::isocket& sk,
                                                       beltpp::isocket::peer_id const& _peerid)
    : meshpp::session_action<meshpp::session_header>()
    , pimpl(&impl)
    , psk(&sk)
    , peerid(_peerid)
    , uri(_uri)
{}

session_action_delete_file::~session_action_delete_file()
{
    if (size_t(-1) != expected_next_package_type ||
        errored)
    {
        BlockchainMessage::RemoteError msg;
        msg.message = "unknown error deleting the file";
        psk->send(peerid, beltpp::packet(std::move(msg)));
    }
}

void session_action_delete_file::initiate(meshpp::session_header&/* header*/)
{
    StorageFileDelete msg;
    msg.uri = uri;
    pimpl->m_slave_node->send(beltpp::packet(std::move(msg)));
    pimpl->m_slave_node->wake();
    expected_next_package_type = BlockchainMessage::Done::rtt;
}

bool session_action_delete_file::process(beltpp::packet&& package, meshpp::session_header&/* header*/)
{
    bool code = true;

    beltpp::on_failure guard([this]{ errored = true; });

    if (expected_next_package_type == package.type() &&
        expected_next_package_type != size_t(-1))
    {
        switch (package.type())
        {
        case BlockchainMessage::Done::rtt:
        {
            psk->send(peerid, beltpp::packet(BlockchainMessage::Done()));

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
        psk->send(peerid, std::move(package));
        completed = true;
        expected_next_package_type = size_t(-1);
    }

    guard.dismiss();

    return code;
}

bool session_action_delete_file::permanent() const
{
    return false;
}

}

