#include "communication_rpc.hpp"

#include "common.hpp"
#include "exception.hpp"
#include "message.tmpl.hpp"

#include "sessions.hpp"

#include <stack>

using std::stack;

namespace publiqpp
{
size_t get_action_size(beltpp::packet const& package)
{
    if (package.type() == BlockLog::rtt)
    {
        BlockLog block_log;
        package.get(block_log);

        return 1 +
               block_log.rewards.size() +
               block_log.transactions.size() +
               block_log.unit_uri_impacts.size() + 
               block_log.applied_sponsor_items.size();
    }

    return 1;
}

void get_actions(LoggedTransactionsRequest const& msg_get_actions,
                 publiqpp::action_log& action_log,
                 beltpp::stream& sk,
                 beltpp::stream::peer_id const& peerid)
{
    uint64_t start_index = msg_get_actions.start_index;

    stack<LoggedTransaction> action_stack;

    size_t count = 0;
    size_t i = start_index;
    size_t len = action_log.length();
    size_t max_count = msg_get_actions.max_count < ACTION_LOG_MAX_RESPONSE ?
                       msg_get_actions.max_count : ACTION_LOG_MAX_RESPONSE;

    bool revert = i < len;
    while (revert && count < max_count) //the case when next action is revert
    {
        LoggedTransaction action_info;
        action_log.at(i, action_info);

        revert = (action_info.logging_type == LoggingType::revert && i < len);

        if (revert)
        {
            count += get_action_size(action_info.action);

            action_info.index = i;
            action_stack.push(std::move(action_info));

            ++i;
        }
    }

    for (; i < len && count < max_count; ++i)
    {
        LoggedTransaction action_info;
        action_log.at(i, action_info);

        // remove all not received entries and their reverts
        if (action_info.logging_type == LoggingType::revert && action_info.index >= start_index)
        {
            count -= get_action_size(action_stack.top().action);

            action_stack.pop();
        }
        else
        {
            count += get_action_size(action_info.action);

            action_info.index = i;
            action_stack.push(std::move(action_info));
        }
    }

    LoggedTransactions msg_actions;
    len = action_stack.size();
    msg_actions.actions.resize(len);

    for (i = len - 1; i < len; --i)
    {
        msg_actions.actions[i] = std::move(action_stack.top());
        action_stack.pop();
    }
    assert(action_stack.empty());

    sk.send(peerid, beltpp::packet(std::move(msg_actions)));
}

void get_hash(DigestRequest&& msg_get_hash,
              beltpp::stream& sk,
              beltpp::stream::peer_id const& peerid)
{
    Digest msg_hash_result;
    msg_hash_result.base58_hash = meshpp::hash(msg_get_hash.package.to_string());
    msg_hash_result.package = std::move(msg_get_hash.package);

    sk.send(peerid, beltpp::packet(std::move(msg_hash_result)));
}

void get_random_seed(beltpp::stream& sk,
                     beltpp::stream::peer_id const& peerid)
{
    meshpp::random_seed rs;
    MasterKey rs_msg;
    rs_msg.master_key = rs.get_brain_key();

    sk.send(peerid, beltpp::packet(std::move(rs_msg)));
}

void get_public_addresses(beltpp::stream& sk,
                          beltpp::stream::peer_id const& peerid,
                          publiqpp::detail::node_internals& impl)
{
    PublicAddressesInfo result = impl.m_nodeid_service.get_addresses();

    sk.send(peerid, beltpp::packet(std::move(result)));
}

void get_peers_addresses(beltpp::stream& sk,
                         beltpp::stream::peer_id const& peerid,
                         publiqpp::detail::node_internals& impl)
{
    PublicAddressesInfo result;
    for (auto const& item : impl.m_p2p_peers)
    {
        PublicAddressInfo info;
        info.ip_address = impl.m_ptr_p2p_socket->info_connection(item);
        info.node_address = item;
        info.seconds_since_checked = 0;
        result.addresses_info.push_back(std::move(info));
    }

    sk.send(peerid, beltpp::packet(std::move(result)));
}

void get_key_pair(KeyPairRequest const& kpr_msg,
                  beltpp::stream& sk,
                  beltpp::stream::peer_id const& peerid)
{
    meshpp::random_seed rs(kpr_msg.master_key);
    meshpp::private_key pv = rs.get_private_key(kpr_msg.index);
    meshpp::public_key pb = pv.get_public_key();

    KeyPair kp_msg;
    kp_msg.master_key = rs.get_brain_key();
    kp_msg.private_key = pv.get_base58_wif();
    kp_msg.public_key = pb.to_string();
    kp_msg.index = kpr_msg.index;

    sk.send(peerid, beltpp::packet(std::move(kp_msg)));
}

void get_signature(SignRequest&& msg,
                   beltpp::stream& sk,
                   beltpp::stream::peer_id const& peerid)
{
    meshpp::private_key pv(msg.private_key);
    meshpp::signature signed_msg = pv.sign(msg.package.to_string());

    Signature sg_msg;
    sg_msg.package = std::move(msg.package);
    sg_msg.signature = signed_msg.base58;
    sg_msg.public_key = pv.get_public_key().to_string();

    sk.send(peerid, beltpp::packet(std::move(sg_msg)));
}

void verify_signature(Signature const& msg,
                      beltpp::stream& sk,
                      beltpp::stream::peer_id const& peerid)
{
    meshpp::signature signed_msg(msg.public_key, msg.package.to_string(), msg.signature);

    sk.send(peerid, beltpp::packet(Done()));
}

void broadcast_message(BlockchainMessage::Broadcast&& broadcast_msg,
                       publiqpp::detail::node_internals& impl)
{
    auto const& all_peers = impl.m_p2p_peers;
    auto const& self = impl.front_public_key().to_string();
    auto const& destination = broadcast_msg.destination;

    // message reached destination
    if (!destination.empty() && self == destination)
        return;

    unordered_set<string> filtered_peers;
 
    if (!destination.empty() && all_peers.count(destination) == 1)
        filtered_peers.insert(destination);
    else
    {
        filtered_peers = all_peers;

        // remove peers from message exclusion
        for (auto const& peer : broadcast_msg.exclusion)
            filtered_peers.erase(peer);

        broadcast_msg.exclusion.clear();
        broadcast_msg.exclusion.push_back(self);

        for (auto const& peer : all_peers)
            broadcast_msg.exclusion.push_back(peer);
    }

    for (auto const& peer : filtered_peers)
    { 
        vector<unique_ptr<meshpp::session_action<meshpp::nodeid_session_header>>> actions;
        actions.emplace_back(new session_action_broadcast(impl, broadcast_msg));
 
        meshpp::nodeid_session_header header;
        header.nodeid = peer;
        header.peerid = peer; // like p2p connection is set
        header.address = impl.m_ptr_p2p_socket->info_connection(peer);
        impl.m_nodeid_sessions.add(header,
                                   std::move(actions),
                                   chrono::seconds(15));
    }
}

}// end of namespace publiqpp
