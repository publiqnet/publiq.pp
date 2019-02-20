#include "communication_rpc.hpp"

#include "common.hpp"
#include "exception.hpp"

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

        return 1 + block_log.rewards.size() + block_log.transactions.size();
    }

    return 1;
}

void get_actions(LoggedTransactionsRequest const& msg_get_actions,
                 publiqpp::action_log& action_log,
                 beltpp::isocket& sk,
                 beltpp::isocket::peer_id const& peerid)
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

    sk.send(peerid, std::move(msg_actions));
}

void get_hash(DigestRequest&& msg_get_hash,
              beltpp::isocket& sk,
              beltpp::isocket::peer_id const& peerid)
{
    Digest msg_hash_result;
    msg_hash_result.base58_hash = meshpp::hash(msg_get_hash.package.to_string());
    msg_hash_result.package = std::move(msg_get_hash.package);

    sk.send(peerid, std::move(msg_hash_result));
}

void get_random_seed(beltpp::isocket& sk,
                     beltpp::isocket::peer_id const& peerid)
{
    meshpp::random_seed rs;
    MasterKey rs_msg;
    rs_msg.master_key = rs.get_brain_key();

    sk.send(peerid, std::move(rs_msg));
}

void get_key_pair(KeyPairRequest const& kpr_msg,
                  beltpp::isocket& sk,
                  beltpp::isocket::peer_id const& peerid)
{
    meshpp::random_seed rs(kpr_msg.master_key);
    meshpp::private_key pv = rs.get_private_key(kpr_msg.index);
    meshpp::public_key pb = pv.get_public_key();

    KeyPair kp_msg;
    kp_msg.master_key = rs.get_brain_key();
    kp_msg.private_key = pv.get_base58_wif();
    kp_msg.public_key = pb.to_string();
    kp_msg.index = kpr_msg.index;

    sk.send(peerid, std::move(kp_msg));
}

void get_signature(SignRequest&& msg,
                   beltpp::isocket& sk,
                   beltpp::isocket::peer_id const& peerid)
{
    meshpp::private_key pv(msg.private_key);
    meshpp::signature signed_msg = pv.sign(msg.package.to_string());

    Signature sg_msg;
    sg_msg.package = std::move(msg.package);
    sg_msg.signature = signed_msg.base58;
    sg_msg.public_key = pv.get_public_key().to_string();

    sk.send(peerid, std::move(sg_msg));
}

void verify_signature(Signature const& msg,
                      beltpp::isocket& sk,
                      beltpp::isocket::peer_id const& peerid)
{
    meshpp::signature signed_msg(msg.public_key, msg.package.to_string(), msg.signature);

    sk.send(peerid, Done());
}

bool process_transfer(BlockchainMessage::SignedTransaction const& signed_transaction,
                      BlockchainMessage::Transfer const& transfer,
                      std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    // Authority check
    if (signed_transaction.authority != transfer.from)
        throw authority_exception(signed_transaction.authority, transfer.from);

    meshpp::public_key pb_key_to(transfer.to);
    meshpp::public_key pb_key_from(transfer.from);

    if (transfer.message.size() > 80)
        throw too_long_string(transfer.message, 80);

    // Don't need to store transaction if sync in process
    // and seems is too far from current block.
    // Just will check the transaction and broadcast
    if (m_pimpl->sync_headers.size() > BLOCK_TR_LENGTH)
        return true;

    // Check pool
    string tr_hash = meshpp::hash(signed_transaction.to_string());

    if (m_pimpl->m_transaction_pool.contains(tr_hash) ||
        m_pimpl->m_transaction_cache.find(tr_hash) != m_pimpl->m_transaction_cache.end())
        return false;

    beltpp::on_failure guard([&m_pimpl] { m_pimpl->discard(); });

    // Validate and add to state
    m_pimpl->m_state.apply_transfer(transfer, signed_transaction.transaction_details.fee);

    // Add to the pool
    m_pimpl->m_transaction_pool.insert(signed_transaction);

    // Add to action log
    m_pimpl->m_action_log.log_transaction(signed_transaction);

    m_pimpl->save(guard);

    return true;
}

bool process_file(BlockchainMessage::SignedTransaction const& signed_transaction,
                  BlockchainMessage::File const& file,
                  std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    // Authority check
    if (signed_transaction.authority != file.author)
        throw authority_exception(signed_transaction.authority, file.author);

    meshpp::public_key pb_key_to(file.author);

    // Don't need to store transaction if sync in process
    // and seems is too far from current block.
    // Just will check the transaction and broadcast
    if (m_pimpl->sync_headers.size() > BLOCK_TR_LENGTH)
        return true;

    // Check pool
    string tr_hash = meshpp::hash(signed_transaction.to_string());

    if (m_pimpl->m_transaction_pool.contains(tr_hash) ||
        m_pimpl->m_transaction_cache.find(tr_hash) != m_pimpl->m_transaction_cache.end())
        return false;

    beltpp::on_failure guard([&m_pimpl] { m_pimpl->discard(); });

    // Validate and add to state
    //m_pimpl->m_state.apply_transfer(transfer, signed_transaction.transaction_details.fee);

    Coin balance = m_pimpl->m_state.get_balance(file.author);
    if (coin(balance) < /*transfer.amount + */signed_transaction.transaction_details.fee)
        throw not_enough_balance_exception(coin(balance), /*transfer.amount + */signed_transaction.transaction_details.fee);

    // decrease "from" balance
    //m_pimpl->m_state.decrease_balance(file.author, 0);

    // Add to the pool
    m_pimpl->m_transaction_pool.insert(signed_transaction);

    // Add to action log
    m_pimpl->m_action_log.log_transaction(signed_transaction);

    m_pimpl->save(guard);

    return true;
}

bool process_content(BlockchainMessage::SignedTransaction const& signed_transaction,
                     BlockchainMessage::Content const& content,
                     std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    // Authority check
    if (signed_transaction.authority != content.channel)
        throw authority_exception(signed_transaction.authority, content.channel);

    meshpp::public_key pb_key_to(content.channel);

    // Don't need to store transaction if sync in process
    // and seems is too far from current block.
    // Just will check the transaction and broadcast
    if (m_pimpl->sync_headers.size() > BLOCK_TR_LENGTH)
        return true;

    // Check pool
    string tr_hash = meshpp::hash(signed_transaction.to_string());

    if (m_pimpl->m_transaction_pool.contains(tr_hash) ||
        m_pimpl->m_transaction_cache.find(tr_hash) != m_pimpl->m_transaction_cache.end())
        return false;

    beltpp::on_failure guard([&m_pimpl] { m_pimpl->discard(); });

    // Validate and add to state
    //m_pimpl->m_state.apply_transfer(transfer, signed_transaction.transaction_details.fee);

    Coin balance = m_pimpl->m_state.get_balance(content.channel);
    if (coin(balance) < /*transfer.amount + */signed_transaction.transaction_details.fee)
        throw not_enough_balance_exception(coin(balance), /*transfer.amount + */signed_transaction.transaction_details.fee);

    // decrease "from" balance
    //m_pimpl->m_state.decrease_balance(file.author, 0);

    // Add to the pool
    m_pimpl->m_transaction_pool.insert(signed_transaction);

    // Add to action log
    m_pimpl->m_action_log.log_transaction(signed_transaction);

    m_pimpl->save(guard);

    return true;
}

void broadcast_message(BlockchainMessage::Broadcast&& broadcast,
                       beltpp::isocket::peer_id const& self,
                       beltpp::isocket::peer_id const& from,
                       bool from_rpc,
                       beltpp::ilog* plog,
                       std::unordered_set<beltpp::isocket::peer_id> const& all_peers,
                       beltpp::isocket* psk)
{
    auto str_compare = [](string const& first, string const& second)
    {
        auto res = first.compare(second);
        if (res > 0)
            res = 1;
        if (res < 0)
            res = -1;
        return res;
    };

    int direction = 0;
    if (false == from_rpc)
    {
        direction = str_compare(self, from);
        if (plog)
            plog->message(self.substr(0, 8) + ", " + from.substr(0, 8) + ": " + std::to_string(direction));

        if (0 == direction)
            return;
    }

    bool chance_to_reflect = beltpp::chance_one_of(10);

    if (from_rpc)
    {
        if (plog)
            plog->message("will broadcast to all");
        broadcast.echoes = 2;
    }
    if (chance_to_reflect)
    {
        if (plog)
            plog->message("can reflect, 1 chance out of 10");
        if (broadcast.echoes == 0)
        {
            if (plog)
                plog->message("    oh no! reflections are expired");
            chance_to_reflect = false;
        }
    }
    if (broadcast.echoes > 2)
        broadcast.echoes = 2;

    auto filtered_peers = all_peers;
    for (auto const& peer : all_peers)
    {
        auto direction2 = str_compare(self, peer);
        if (false == from_rpc &&
            direction == direction2)
            filtered_peers.erase(peer);
    }

    if (filtered_peers.empty() &&
        chance_to_reflect)
    {
        if (plog)
            plog->message("since all peers would be skipped by reflection, "
                          "use the chance to reflect, and broadcast to everyone");
        filtered_peers = all_peers;
        --broadcast.echoes;
    }
    else if (filtered_peers.empty())
    {
        if (plog)
            plog->message("all peers will be skipped by direction");
    }

    for (auto const& peer : filtered_peers)
    {
        if (plog)
            plog->message("will rebroadcast to: " + peer);

        psk->send(peer, broadcast);
    }
}

bool do_i_need_it(BlockchainMessage::ArticleInfo article_info,
                  std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    //TODO
    B_UNUSED(article_info);

    return m_pimpl->m_node_type == NodeType::storage;
}
}// end of namespace publiqpp
