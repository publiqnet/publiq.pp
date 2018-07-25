#include "node.hpp"

#include "state.hpp"
#include "blockchain.hpp"
#include "transaction_pool.hpp"
#include "action_log.hpp"
#include "storage.hpp"
#include "message.hpp"
#include "communication_rpc.hpp"
#include "open_container_packet.hpp"
#include "http.hpp"

#include <belt.pp/packet.hpp>
#include <belt.pp/utility.hpp>
#include <belt.pp/event.hpp>

#include <belt.pp/socket.hpp>
#include <belt.pp/packet.hpp>
#include <belt.pp/message_global.hpp>
#include <belt.pp/scope_helper.hpp>
#include <belt.pp/utility.hpp>

#include <mesh.pp/p2psocket.hpp>
#include <mesh.pp/cryptoutility.hpp>

#include <boost/filesystem/path.hpp>

#include <utility>
#include <exception>
#include <string>
#include <memory>
#include <chrono>
#include <unordered_set>
#include <unordered_map>
#include <vector>

using namespace BlockchainMessage;

namespace filesystem = boost::filesystem;

using beltpp::ip_address;
using beltpp::ip_destination;
using beltpp::socket;
using beltpp::packet;
using peer_id = socket::peer_id;
using std::unordered_set;
using std::unordered_map;
using std::pair;

namespace chrono = std::chrono;
using chrono::system_clock;
using chrono::steady_clock;
using std::string;
using std::vector;
using std::unique_ptr;

//  MSVS does not instansiate template function only because its address
//  is needed, so let's force it
template beltpp::void_unique_ptr beltpp::new_void_unique_ptr<Error>();
template beltpp::void_unique_ptr beltpp::new_void_unique_ptr<Join>();
template beltpp::void_unique_ptr beltpp::new_void_unique_ptr<Drop>();

namespace publiqpp
{

using p2p_sf = meshpp::p2psocket_family_t<
    Error::rtt,
    Join::rtt,
    Drop::rtt,
    &beltpp::new_void_unique_ptr<Error>,
    &beltpp::new_void_unique_ptr<Join>,
    &beltpp::new_void_unique_ptr<Drop>,
    &Error::saver,
    &Join::saver,
    &Drop::saver
>;

using rpc_sf = beltpp::socket_family_t<
    Error::rtt,
    Join::rtt,
    Drop::rtt,
    &beltpp::new_void_unique_ptr<Error>,
    &beltpp::new_void_unique_ptr<Join>,
    &beltpp::new_void_unique_ptr<Drop>,
    &Error::saver,
    &Join::saver,
    &Drop::saver,
    &http::message_list_load
>;

namespace detail
{
class packet_and_expiry
{
public:
    beltpp::packet packet;
    size_t expiry;
};

beltpp::void_unique_ptr get_putl()
{
    beltpp::message_loader_utility utl;
    BlockchainMessage::detail::extension_helper(utl);

    auto ptr_utl =
            beltpp::new_void_unique_ptr<beltpp::message_loader_utility>(std::move(utl));

    return ptr_utl;
}
class node_internals
{
public:
    node_internals(ip_address const& rpc_bind_to_address,
                   ip_address const& p2p_bind_to_address,
                   std::vector<ip_address> const& p2p_connect_to_addresses,
                   filesystem::path const& fs_blockchain,
                   filesystem::path const& fs_action_log,
                   filesystem::path const& fs_storage,
                   filesystem::path const& fs_transaction_pool,
                   filesystem::path const& fs_state,
                   beltpp::ilog* _plogger_p2p,
                   beltpp::ilog* _plogger_node)
        : plogger_p2p(_plogger_p2p)
        , plogger_node(_plogger_node)
        , m_ptr_eh(new beltpp::event_handler())
        , m_ptr_p2p_socket(new meshpp::p2psocket(
                               meshpp::getp2psocket<p2p_sf>(*m_ptr_eh,
                                                            p2p_bind_to_address,
                                                            p2p_connect_to_addresses,
                                                            get_putl(),
                                                            _plogger_p2p)
                               ))
        , m_ptr_rpc_socket(new beltpp::socket(
                               beltpp::getsocket<rpc_sf>(*m_ptr_eh)
                               ))

        , m_blockchain(fs_blockchain)
        , m_action_log(fs_action_log)
        , m_storage(fs_storage)
        , m_transaction_pool(fs_transaction_pool)
        , m_state(fs_state)
    {
        m_ptr_eh->set_timer(chrono::seconds(30));

        m_ptr_rpc_socket->listen(rpc_bind_to_address);

        m_ptr_eh->add(*m_ptr_rpc_socket);
        m_ptr_eh->add(*m_ptr_p2p_socket);
    }

    void write_p2p(string const& value)
    {
        if (plogger_p2p)
            plogger_p2p->message_no_eol(value);
    }

    void writeln_p2p(string const& value)
    {
        if (plogger_p2p)
            plogger_p2p->message(value);
    }

    void write_node(string const& value)
    {
        if (plogger_node)
            plogger_node->message_no_eol(value);
    }

    void writeln_node(string const& value)
    {
        if (plogger_node)
            plogger_node->message(value);
    }

    void add_peer(socket::peer_id const& peerid)
    {
        pair<unordered_set<socket::peer_id>::iterator, bool> result =
                m_p2p_peers.insert(peerid);

        if (result.second == false)
            throw std::runtime_error("p2p peer already exists: " + peerid);
    }

    void remove_peer(socket::peer_id const& peerid)
    {
        reset_stored_request(peerid);
        if (0 == m_p2p_peers.erase(peerid))
            throw std::runtime_error("p2p peer not found to remove: " + peerid);
    }

    void find_stored_request(socket::peer_id const& peerid,
                             beltpp::packet& packet)
    {
        auto it = m_stored_requests.find(peerid);
        if (it != m_stored_requests.end())
        {
            BlockchainMessage::detail::assign_packet(packet, it->second.packet);
        }
    }
    void reset_stored_request(beltpp::isocket::peer_id const& peerid)
    {
        m_stored_requests.erase(peerid);
    }

    void store_request(socket::peer_id const& peerid,
                       beltpp::packet const& packet)
    {
        detail::packet_and_expiry pck;
        BlockchainMessage::detail::assign_packet(pck.packet, packet);
        pck.expiry = 2;
        auto res = m_stored_requests.insert(std::make_pair(peerid, std::move(pck)));
        if (false == res.second)
            throw std::runtime_error("only one request is supported at a time");
    }

    void clear_state()
    {
        block_vector.clear();
        header_vector.clear();
    }

    std::vector<beltpp::isocket::peer_id> do_step()
    {
        vector<beltpp::isocket::peer_id> result;

        for (auto& key_value : m_stored_requests)
        {
            if (0 == key_value.second.expiry)
                result.push_back(key_value.first);

            --key_value.second.expiry;
        }
        return result;
    }

    beltpp::ilog* plogger_p2p;
    beltpp::ilog* plogger_node;
    unique_ptr<beltpp::event_handler> m_ptr_eh;
    unique_ptr<meshpp::p2psocket> m_ptr_p2p_socket;
    unique_ptr<beltpp::socket> m_ptr_rpc_socket;

    publiqpp::blockchain m_blockchain;
    publiqpp::action_log m_action_log;
    publiqpp::storage m_storage;
    publiqpp::transaction_pool m_transaction_pool;
    publiqpp::state m_state;

    unordered_set<beltpp::isocket::peer_id> m_p2p_peers;
    unordered_map<beltpp::isocket::peer_id, packet_and_expiry> m_stored_requests;

    vector<SignedBlock> block_vector;
    vector<BlockHeader> header_vector;
    vector<std::pair<beltpp::isocket::peer_id, SyncResponse>> sync_vector;
};
}

/*
 * node
 */
node::node(ip_address const& rpc_bind_to_address,
           ip_address const& p2p_bind_to_address,
           std::vector<ip_address> const& p2p_connect_to_addresses,
           boost::filesystem::path const& fs_blockchain,
           boost::filesystem::path const& fs_action_log,
           boost::filesystem::path const& fs_storage,
           boost::filesystem::path const& fs_transaction_pool,
           boost::filesystem::path const& fs_state,
           beltpp::ilog* plogger_p2p,
           beltpp::ilog* plogger_node)
    : m_pimpl(new detail::node_internals(rpc_bind_to_address,
                                         p2p_bind_to_address,
                                         p2p_connect_to_addresses,
                                         fs_blockchain,
                                         fs_action_log,
                                         fs_storage,
                                         fs_transaction_pool,
                                         fs_state,
                                         plogger_p2p,
                                         plogger_node))
{

}

node::node(node&&) = default;

node::~node()
{

}

/*void node::send(peer_id const& peer,
                packet&& pack)
{
    Other wrapper;
    wrapper.contents = std::move(pack);
    m_pimpl->m_ptr_p2p_socket->send(peer, std::move(wrapper));
}*/

void node::terminate()
{
    m_pimpl->m_ptr_eh->terminate();
}

string node::name() const
{
    return m_pimpl->m_ptr_p2p_socket->name();
}

bool node::run()
{
    bool code = true;

    unordered_set<beltpp::ievent_item const*> wait_sockets;

    //m_pimpl->writeln_node("eh.wait");
    auto wait_result = m_pimpl->m_ptr_eh->wait(wait_sockets);
    //m_pimpl->writeln_node("eh.wait - done");

    enum class interface_type {p2p, rpc};

    if (wait_result == beltpp::event_handler::event)
    {
        for (auto& pevent_item : wait_sockets)
        {
            interface_type it = interface_type::rpc;
            if (pevent_item == &m_pimpl->m_ptr_p2p_socket.get()->worker())
                it = interface_type::p2p;

            auto str_receive = [it]
            {
                if (it == interface_type::p2p)
                    return "p2p_sk.receive";
                else
                    return "rpc_sk.receive";
            };
            str_receive();

            auto str_peerid = [it](string const& peerid)
            {
                if (it == interface_type::p2p)
                    return peerid.substr(0, 5);
                else
                    return peerid;
            };

            beltpp::socket::peer_id peerid;

            beltpp::isocket* psk = nullptr;
            if (pevent_item == &m_pimpl->m_ptr_p2p_socket->worker())
                psk = m_pimpl->m_ptr_p2p_socket.get();
            else if (pevent_item == m_pimpl->m_ptr_rpc_socket.get())
                psk = m_pimpl->m_ptr_rpc_socket.get();

            beltpp::socket::packets received_packets;
            if (psk != nullptr)
                received_packets = psk->receive(peerid);

            for (auto& received_packet : received_packets)
            {
            try
            {
                vector<packet*> composition;

                open_container_packet<Broadcast, SignedTransaction> broadcast_transaction;
                open_container_packet<Broadcast, SignedBlock> broadcast_block;
                open_container_packet<Broadcast> broadcast_anything;
                bool is_container =
                        (broadcast_transaction.open(received_packet, composition) ||
                         broadcast_block.open(received_packet, composition) ||
                         broadcast_anything.open(received_packet, composition));

                if (is_container == false)
                {
                    composition.clear();
                    composition.push_back(&received_packet);
                }

                packet& ref_packet = *composition.back();

                packet stored_packet;
                if (it == interface_type::p2p)
                    m_pimpl->find_stored_request(peerid, stored_packet);
                
                switch (ref_packet.type())
                {
                case Join::rtt:
                {
                    m_pimpl->write_node(str_peerid(peerid));
                    m_pimpl->writeln_node("joined");

                    if (psk == m_pimpl->m_ptr_p2p_socket.get())
                    {
                        beltpp::on_failure guard(
                            [&peerid, &psk] { psk->send(peerid, Drop()); });

                        m_pimpl->add_peer(peerid);

                        guard.dismiss();

                        m_pimpl->store_request(peerid, ChainInfoRequest());
                        psk->send(peerid, ChainInfoRequest());
                    }

                    break;
                }
                case Drop::rtt:
                {
                    m_pimpl->write_node(str_peerid(peerid));
                    m_pimpl->writeln_node("dropped");

                    if (psk == m_pimpl->m_ptr_p2p_socket.get())
                        m_pimpl->remove_peer(peerid);

                    break;
                }
                case Error::rtt:
                {
                    m_pimpl->write_node(str_peerid(peerid));
                    m_pimpl->writeln_node("error");
                    psk->send(peerid, Drop());

                    if (psk == m_pimpl->m_ptr_p2p_socket.get())
                        m_pimpl->remove_peer(peerid);

                    break;
                }
                case Shutdown::rtt:
                {
                    if (it != interface_type::rpc)
                        break;

                    m_pimpl->writeln_node("shutdown received");

                    code = false;

                    psk->send(peerid, Done());

                    if (false == broadcast_anything.items.empty())
                    {
                        m_pimpl->writeln_node("broadcasting shutdown");

                        Shutdown shutdown_msg;
                        ref_packet.get(shutdown_msg);

                        for (auto const& p2p_peer : m_pimpl->m_p2p_peers)
                            m_pimpl->m_ptr_p2p_socket->send(p2p_peer, shutdown_msg);
                    }
                    break;
                }
                case Transfer::rtt:
                {
                    if (broadcast_transaction.items.empty())
                        throw std::runtime_error("will process only \"broadcast signed transfer\"");
                
                    process_transfer(*broadcast_transaction.items[1],
                                     ref_packet,
                                     m_pimpl->m_action_log,
                                     m_pimpl->m_transaction_pool,
                                     m_pimpl->m_state);


                    broadcast(received_packet,
                              m_pimpl->m_ptr_p2p_socket->name(),
                              peerid,
                              (it == interface_type::rpc),
                              m_pimpl->plogger_node,
                              m_pimpl->m_p2p_peers,
                              m_pimpl->m_ptr_p2p_socket.get());
                
                    psk->send(peerid, Done());
                    break;
                }
                case Block::rtt:
                {
                    if (broadcast_block.items.empty())
                        throw std::runtime_error("will process only \"broadcast signed block\"");

                    // Verify block signature
                    SignedBlock signed_block;
                    broadcast_block.items[1]->get(signed_block);

                    meshpp::signature sg(meshpp::public_key(signed_block.authority), signed_block.block_details.save(), signed_block.signature);

                    sg.check();

                    //Check consensus
                    //TODO

                    std::vector<beltpp::packet> package_blocks;
                    package_blocks.push_back(std::move(ref_packet));

                    insert_blocks(package_blocks,
                                  m_pimpl->m_action_log,
                                  m_pimpl->m_transaction_pool,
                                  m_pimpl->m_state);

                    //TODO add to blockchain

                    //TODO broadcast

                    psk->send(peerid, Done());

                    //TODO manage exceptions

                    break;
                }
                case ChainInfoRequest::rtt:
                {
                    ChainInfo chaininfo_msg;
                    chaininfo_msg.length = m_pimpl->m_blockchain.length();
                    psk->send(peerid, chaininfo_msg);
                    break;
                }
                case ChainInfo::rtt:
                {
                    if (it == interface_type::p2p)
                    {
                        m_pimpl->reset_stored_request(peerid);
                        if (stored_packet.type() != ChainInfoRequest::rtt)
                            throw std::runtime_error("I didn't ask for chain info");
                    }
                    break;
                }
                case LogTransaction::rtt:
                {
                    if (it == interface_type::rpc)
                        submit_action(std::move(ref_packet),
                                      m_pimpl->m_action_log,
                                      m_pimpl->m_transaction_pool,
                                      *psk,
                                      peerid);
                    break;
                }
                case RevertLastLoggedAction::rtt:
                {
                    if (it == interface_type::rpc)
                    {
                        m_pimpl->m_action_log.revert();
                        psk->send(peerid, Done());
                    }
                    break;
                }
                case LoggedTransactionsRequest::rtt:
                {
                    if (it == interface_type::rpc)
                        get_actions(ref_packet, m_pimpl->m_action_log, *psk, peerid);
                    break;
                }
                case DigestRequest::rtt:
                {
                    get_hash(ref_packet, *psk, peerid);
                    break;
                }
                case MasterKeyRequest::rtt:
                {
                    get_random_seed(*psk, peerid);
                    break;
                }
                case KeyPairRequest::rtt:
                {
                    get_key_pair(ref_packet, *psk, peerid);
                    break;
                }
                case SignRequest::rtt:
                {
                    get_signature(ref_packet, *psk, peerid);
                    break;
                }
                case Signature::rtt:
                {
                    verify_signature(ref_packet, *psk, peerid);
                    break;
                }
                case SyncRequest::rtt:
                {
                    if (it == interface_type::p2p)
                    {
                        SyncRequest sync_request;
                        std::move(ref_packet).get(sync_request);

                        BlockHeader block_header;
                        m_pimpl->m_blockchain.header(block_header);

                        SyncResponse sync_response;
                        sync_response.block_number = block_header.block_number;
                        sync_response.consensus_sum = block_header.consensus_sum;

                        if (sync_response.block_number > sync_request.block_number ||
                            (sync_response.block_number == sync_request.block_number &&
                             sync_response.consensus_sum > sync_request.consensus_sum))
                        {
                            psk->send(peerid, std::move(sync_response));
                        }
                    }
                    else
                    {
                        RemoteError remote_error;
                        remote_error.message = "Wrong request!";
                        psk->send(peerid, remote_error);
                    }
                    break;
                }
                case SyncResponse::rtt:
                {
                    if (it == interface_type::p2p)
                    {
                        //check if request was sent
                        //TODO

                        SyncResponse sync_response;
                        std::move(ref_packet).get(sync_response);

                        m_pimpl->sync_vector.push_back(std::pair<beltpp::isocket::peer_id, SyncResponse>(peerid, sync_response));
                    }
                    else
                    {
                        RemoteError remote_error;
                        remote_error.message = "Wrong request!";
                        psk->send(peerid, remote_error);
                    }
                    break;
                }
                case ConsensusRequest::rtt:
                {
                    if (it == interface_type::p2p)
                    {
                        ConsensusRequest consensus_request;
                        std::move(ref_packet).get(consensus_request);

                        BlockHeader tmp_header;

                        if (m_pimpl->m_blockchain.tmp_header(tmp_header))
                        {
                            if (tmp_header.block_number < consensus_request.block_number ||
                                (tmp_header.block_number == consensus_request.block_number &&
                                    tmp_header.consensus_delta < consensus_request.consensus_delta))
                            {
                                // someone have better block
                                m_pimpl->m_blockchain.step_disable();
                            }
                            else
                            {
                                // I have better block
                                ConsensusResponse consensus_response;
                                consensus_response.block_number = tmp_header.block_number;
                                consensus_response.consensus_delta = tmp_header.consensus_delta;

                                psk->send(peerid, consensus_response);
                            }
                        }
                    }
                    else
                    {
                        RemoteError remote_error;
                        remote_error.message = "Wrong request!";
                        psk->send(peerid, remote_error);
                    }
                    break;
                }
                case ConsensusResponse::rtt:
                {
                    if (it == interface_type::p2p)
                    {
                        //check if request was sent
                        //TODO

                        ConsensusResponse consensus_response;
                        std::move(ref_packet).get(consensus_response);

                        BlockHeader tmp_header;

                        if (m_pimpl->m_blockchain.tmp_header(tmp_header))
                        {
                            if (tmp_header.block_number < consensus_response.block_number ||
                                (tmp_header.block_number == consensus_response.block_number &&
                                    tmp_header.consensus_delta < consensus_response.consensus_delta))
                            {
                                // some peer have better block
                                m_pimpl->m_blockchain.step_disable();
                            }
                        }
                    }
                    else
                    {
                        RemoteError remote_error;
                        remote_error.message = "Wrong request!";
                        psk->send(peerid, remote_error);
                    }
                    break;
                }
                case BlockHeaderRequest::rtt:
                {
                    if (it == interface_type::p2p)
                    {
                        BlockHeaderRequest header_request;
                        std::move(ref_packet).get(header_request);

                        uint64_t from = m_pimpl->m_blockchain.length();
                        from = from < header_request.blocks_from ? from : header_request.blocks_from;

                        uint64_t to = header_request.blocks_to;
                        to = to > from ? from : to;
                        to = to < from - 10 ? from - 10 : to;

                        BlockHeaderResponse header_response;
                        for (auto index = from; index >= to; --to)
                        {
                            SignedBlock signed_block;
                            m_pimpl->m_blockchain.at(index, signed_block);
                            
                            Block block;
                            std::move(signed_block.block_details).get(block);

                            header_response.block_headers.push_back(std::move(block.block_header));
                        }

                        psk->send(peerid, header_response);
                    }
                    else
                    {
                        RemoteError remote_error;
                        remote_error.message = "Wrong request!";
                        psk->send(peerid, remote_error);
                    }
                    break;
                }
                case BlockHeaderResponse::rtt:
                {
                    if (it == interface_type::p2p)
                    {
                        // check if the blockheader was requested
                        //TODO

                        // find needed header from own data
                        BlockHeader tmp_header;
                        m_pimpl->m_blockchain.header(tmp_header);

                        if (!m_pimpl->header_vector.empty() && // we have something received before
                            tmp_header.block_number >= m_pimpl->header_vector.rbegin()->block_number)
                        {
                            // load next mot checked header
                            m_pimpl->m_blockchain.header_at(m_pimpl->header_vector.rbegin()->block_number - 1, tmp_header);
                        }

                        BlockHeaderResponse header_response;
                        std::move(ref_packet).get(header_response);

                        // validate received headers
                        auto it = header_response.block_headers.begin();
                        bool bad_data = header_response.block_headers.empty();
                        bad_data = bad_data ||
                                (!m_pimpl->header_vector.empty() &&
                                 tmp_header.block_number != (*it).block_number);

                        for (++it; !bad_data && it != header_response.block_headers.end(); ++it)
                        {
                            bad_data = bad_data || (*(it - 1)).block_number != (*it).block_number + 1;
                            bad_data = bad_data || (*(it - 1)).consensus_sum <= (*it).consensus_sum;
                            bad_data = bad_data || (*(it - 1)).consensus_sum != (*(it - 1)).consensus_delta + (*it).consensus_sum;
                            bad_data = bad_data || (
                                        (*(it - 1)).consensus_const != (*it).consensus_const &&
                                        (*(it - 1)).consensus_const != 2 * (*it).consensus_const
                                    );
                        }

                        if(bad_data)
                        {
                            psk->send(peerid, Drop());
                            m_pimpl->remove_peer(peerid);
                            m_pimpl->clear_state();
                            break;
                        }

                        // find last common header
                        bool found = false;
                        it = header_response.block_headers.begin();
                        while (!found && it != header_response.block_headers.end())
                        {
                            if (tmp_header.block_number < (*it).block_number)
                            {
                                // store for possible use
                                m_pimpl->header_vector.push_back(std::move(*it));
                                ++it;
                            }
                            else
                                found = true;
                        }

                        bool lcb_found = false;

                        if (found)
                        {
                            for (; !lcb_found && it != header_response.block_headers.end(); ++it)
                            {
                                if (tmp_header == (*it))
                                {
                                    lcb_found = true;
                                    continue;
                                }
                                else if (tmp_header.consensus_sum < (*it).consensus_sum)
                                {
                                    // store for possible use
                                    m_pimpl->header_vector.push_back(std::move(*it));
                                    m_pimpl->m_blockchain.header_at(tmp_header.block_number - 1, tmp_header);
                                }
                            }

                            if (lcb_found)
                            {
                                if (m_pimpl->block_vector.empty())
                                {
                                    // nothing new! interrup connection
                                    psk->send(peerid, Drop());
                                    m_pimpl->remove_peer(peerid);
                                    m_pimpl->clear_state();
                                    break;
                                }

                                //3. request blockchain from found point
                                BlockChainRequest blockchain_request;
                                blockchain_request.blocks_from = m_pimpl->header_vector.begin()->block_number;
                                blockchain_request.blocks_to = m_pimpl->header_vector.rbegin()->block_number;
                            
                                psk->send(peerid, blockchain_request);
                            
                                //TODO store request
                            }
                        }
                        
                        if(!found || !lcb_found)
                        {
                            // request more headers
                            BlockHeaderRequest header_request;
                            header_request.blocks_from = m_pimpl->header_vector.rbegin()->block_number - 1;
                            header_request.blocks_to = header_request.blocks_from - 10;

                            psk->send(peerid, header_request);

                            //TODO store request
                        }
                    }
                    else
                    {
                        RemoteError remote_error;
                        remote_error.message = "Wrong request!";
                        psk->send(peerid, remote_error);
                    }
                    break;
                }
                case BlockChainRequest::rtt:
                {
                    if (it == interface_type::p2p)
                    {
                        BlockChainRequest chain_request;
                        std::move(ref_packet).get(chain_request);

                        uint64_t from = m_pimpl->m_blockchain.length();
                        from = from < chain_request.blocks_from ? from : chain_request.blocks_from;

                        uint64_t to = chain_request.blocks_to;
                        to = to > from ? from : to;
                        to = to < from - 10 ? from - 10 : to;

                        BlockChainResponse chain_response;
                        for (auto index = from; index >= to; --to)
                        {
                            SignedBlock signed_block;
                            m_pimpl->m_blockchain.at(index, signed_block);

                            chain_response.signed_blocks.push_back(std::move(signed_block));
                        }

                        psk->send(peerid, chain_response);
                    }
                    else
                    {
                        RemoteError remote_error;
                        remote_error.message = "Wrong request!";
                        psk->send(peerid, remote_error);
                    }
                    break;
                }
                case BlockChainResponse::rtt:
                {
                    if (it == interface_type::p2p)
                    {
                        //1. check if the chain was requested
                        //TODO

                        //2. check received chain validity
                        BlockChainResponse chain_response;
                        std::move(ref_packet).get(chain_response);
                        //TODO

                        //3. apply received chain
                        //TODO
                    }
                    else
                    {
                        RemoteError remote_error;
                        remote_error.message = "Wrong request!";
                        psk->send(peerid, remote_error);
                    }
                    break;
                }
                default:
                {
                    m_pimpl->writeln_node("don't know how to handle: dropping " + peerid);
                    psk->send(peerid, Drop());

                    if (psk == m_pimpl->m_ptr_p2p_socket.get())
                        m_pimpl->remove_peer(peerid);
                    break;
                }
                }   // switch ref_packet.type()
            }
            catch (meshpp::exception_public_key const& e)
            {
                InvalidPublicKey msg;
                msg.public_key = e.pub_key;
                psk->send(peerid, msg);
                throw;
            }
            catch (meshpp::exception_private_key const& e)
            {
                InvalidPrivateKey msg;
                msg.private_key = e.priv_key;
                psk->send(peerid, msg);
                throw;
            }
            catch (meshpp::exception_signature const& e)
            {
                InvalidSignature msg;
                msg.details.public_key = e.sgn.pb_key.to_string();
                msg.details.signature = e.sgn.base64;
                BlockchainMessage::detail::loader(msg.details.package,
                                                  std::string(e.sgn.message.begin(), e.sgn.message.end()),
                                                  nullptr);

                psk->send(peerid, msg);
                throw;
            }
            catch (exception_authority const& e)
            {
                InvalidAuthority msg;
                msg.authority_provided = e.authority_provided;
                msg.authority_required = e.authority_required;
                psk->send(peerid, msg);
                throw;
            }
            catch(std::exception const& e)
            {
                RemoteError msg;
                msg.message = e.what();
                psk->send(peerid, msg);
                throw;
            }
            catch (...)
            {
                RemoteError msg;
                msg.message = "unknown exception";
                psk->send(peerid, msg);
                throw;
            }
            }   // for (auto& received_packet : received_packets)
        }   // for (auto& pevent_item : wait_sockets)
    }
    else if (beltpp::event_handler::timer_out == wait_result)
    {
        m_pimpl->writeln_node("timer");

        m_pimpl->m_ptr_p2p_socket->timer_action();
        m_pimpl->m_ptr_rpc_socket->timer_action();

        auto const& peerids_to_remove = m_pimpl->do_step();
        for (auto const& peerid_to_remove : peerids_to_remove)
        {
            m_pimpl->writeln_node("not answering: dropping " + peerid_to_remove);
            m_pimpl->m_ptr_p2p_socket->send(peerid_to_remove, Drop());
            m_pimpl->remove_peer(peerid_to_remove);
        }

        beltpp::isocket* psk = m_pimpl->m_ptr_p2p_socket.get();

        // Sync node
        // process collected blocks
        BlockHeader block_header;
        m_pimpl->m_blockchain.header(block_header);

        SyncRequest sync_request;
        sync_request.block_number = block_header.block_number;
        sync_request.consensus_sum = block_header.consensus_sum;
        beltpp::isocket::peer_id tmp_peer = "tmp_peer";

        for (auto& it : m_pimpl->sync_vector)
        {
            if (sync_request.block_number < it.second.block_number ||
                (sync_request.block_number == it.second.block_number &&
                    sync_request.consensus_sum < it.second.consensus_sum))
            {
                sync_request.block_number = it.second.block_number;
                sync_request.consensus_sum = it.second.consensus_sum;
                tmp_peer = it.first;
            }
        }

        m_pimpl->sync_vector.clear();

        if (tmp_peer != "tmp_peer")
        {
            // request better chain
            BlockHeaderRequest header_request;
            header_request.blocks_from = sync_request.block_number;
            header_request.blocks_to = m_pimpl->m_blockchain.length();
            psk->send(tmp_peer, header_request);
        
            //TODO store request
        }
        else
        {
            // new sync request 
            sync_request.block_number = block_header.block_number;
            sync_request.consensus_sum = block_header.consensus_sum;

            for (auto& it : m_pimpl->m_p2p_peers)
                psk->send(it, sync_request);
        }

        // Mine block
        string key; // TODO assign
        uint64_t amount = m_pimpl->m_state.get_balance(key);

        if (m_pimpl->m_blockchain.mine_block(key, amount, m_pimpl->m_transaction_pool))
        {
            BlockHeader tmp_header;

            if (m_pimpl->m_blockchain.tmp_header(tmp_header))
            {
                ConsensusRequest consensus_request;
                consensus_request.block_number = tmp_header.block_number;
                consensus_request.consensus_delta = tmp_header.consensus_delta;

                for (auto& it : m_pimpl->m_p2p_peers)
                    psk->send(it, consensus_request);
            }
        }

        // Apply own block
        m_pimpl->m_blockchain.step_apply();
    }

    return code;
}

}


