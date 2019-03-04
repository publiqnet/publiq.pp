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

bool process_content_unit(BlockchainMessage::SignedTransaction const& signed_transaction,
                          BlockchainMessage::ContentUnit const& content_unit,
                          std::unique_ptr<publiqpp::detail::node_internals>& pimpl)
{
    // Authority check
    if (signed_transaction.authority != content_unit.author_address)
        throw authority_exception(signed_transaction.authority, content_unit.author_address);

    // Check pool
    string tr_hash = meshpp::hash(signed_transaction.to_string());

    if (pimpl->m_transaction_cache.count(tr_hash))
        return false;

    if (pimpl->m_documents.exist_unit(content_unit.uri))
        throw wrong_document_exception("File already exists!");

    Coin balance = pimpl->m_state.get_balance(content_unit.author_address);
    if (coin(balance) < signed_transaction.transaction_details.fee)
        throw not_enough_balance_exception(coin(balance), signed_transaction.transaction_details.fee);

    for(auto uri : content_unit.file_uris)
        if(false == pimpl->m_documents.exist_file(uri))
            throw wrong_document_exception("Missing file with uri : " + uri);

    meshpp::public_key pb_key_author(content_unit.author_address);
    meshpp::public_key pb_key_channel(content_unit.channel_address);

    // Don't need to store transaction if sync in process
    // and seems is too far from current block.
    // Just will check the transaction and broadcast
    if (pimpl->sync_headers.size() > BLOCK_TR_LENGTH)
        return true;

    auto transaction_cache_backup = pimpl->m_transaction_cache;

    beltpp::on_failure guard([&pimpl, &transaction_cache_backup]
    {
        pimpl->discard();
        pimpl->m_transaction_cache = std::move(transaction_cache_backup);
    });

    // Add to documents
    pimpl->m_documents.insert_unit(content_unit);

    // Add to the pool
    pimpl->m_transaction_pool.push_back(signed_transaction);
    pimpl->m_transaction_cache[tr_hash] =
        system_clock::from_time_t(signed_transaction.transaction_details.creation.tm);

    // Add to action log
    pimpl->m_action_log.log_transaction(signed_transaction);

    pimpl->save(guard);

    return true;
}

bool process_content(BlockchainMessage::SignedTransaction const& signed_transaction,
                     BlockchainMessage::Content const& content,
                     std::unique_ptr<publiqpp::detail::node_internals>& pimpl)
{
    // Authority check
    if (signed_transaction.authority != content.channel_address)
        throw authority_exception(signed_transaction.authority, content.channel_address);

    Coin balance = pimpl->m_state.get_balance(content.channel_address);
    if (coin(balance) < signed_transaction.transaction_details.fee)
        throw not_enough_balance_exception(coin(balance), signed_transaction.transaction_details.fee);

    for (auto uri : content.content_unit_uris)
        if (false == pimpl->m_documents.exist_unit(uri))
            throw wrong_document_exception("Missing content_unit with uri : " + uri);

    meshpp::public_key pb_key_channel(content.channel_address);

    // Don't need to store transaction if sync in process
    // and seems is too far from current block.
    // Just will check the transaction and broadcast
    if (pimpl->sync_headers.size() > BLOCK_TR_LENGTH)
        return true;

    // Check pool
    string tr_hash = meshpp::hash(signed_transaction.to_string());

    if (pimpl->m_transaction_cache.count(tr_hash))
        return false;

    auto transaction_cache_backup = pimpl->m_transaction_cache;

    beltpp::on_failure guard([&pimpl, &transaction_cache_backup]
    {
        pimpl->discard();
        pimpl->m_transaction_cache = std::move(transaction_cache_backup);
    });

    // Add to the pool
    pimpl->m_transaction_pool.push_back(signed_transaction);
    pimpl->m_transaction_cache[tr_hash] =
        system_clock::from_time_t(signed_transaction.transaction_details.creation.tm);

    // Add to action log
    pimpl->m_action_log.log_transaction(signed_transaction);

    pimpl->save(guard);

    return true;
}

bool process_content_info(BlockchainMessage::SignedTransaction const& signed_transaction,
                          BlockchainMessage::ContentInfo const& content_info,
                          std::unique_ptr<publiqpp::detail::node_internals>& pimpl)
{
    // Check data and authority
    if (signed_transaction.authority != content_info.storage_address)
        throw authority_exception(signed_transaction.authority, content_info.storage_address);

    NodeType node_type;
    if (false == pimpl->m_state.get_role(signed_transaction.authority, node_type) || node_type != NodeType::storage)
        throw wrong_data_exception("process_content_info -> wrong authority type : " + signed_transaction.authority);

    // Check pool and cache
    string tr_hash = meshpp::hash(signed_transaction.to_string());

    if (pimpl->m_transaction_cache.count(tr_hash))
        return false;

    auto transaction_cache_backup = pimpl->m_transaction_cache;

    beltpp::on_failure guard([&pimpl, &transaction_cache_backup]
    {
        pimpl->discard();
        pimpl->m_transaction_cache = std::move(transaction_cache_backup);
    });

    // Add to the pool
    pimpl->m_transaction_pool.push_back(signed_transaction);
    pimpl->m_transaction_cache[tr_hash] =
        system_clock::from_time_t(signed_transaction.transaction_details.creation.tm);

    // Add to action log
    pimpl->m_action_log.log_transaction(signed_transaction);

    pimpl->save(guard);

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

}// end of namespace publiqpp
