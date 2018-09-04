#include "communication_rpc.hpp"
#include "common.hpp"

#include <stack>

using std::stack;

void get_actions(beltpp::packet const& package,
                 publiqpp::action_log& action_log,
                 beltpp::isocket& sk,
                 beltpp::isocket::peer_id const& peerid)
{
    LoggedTransactionsRequest msg_get_actions;
    package.get(msg_get_actions);
    uint64_t index = msg_get_actions.start_index;

    stack<LoggedTransaction> action_stack;

    size_t i = index;
    size_t len = action_log.length();

    bool revert = i < len;
    while (revert) //the case when next action is revert
    {
        LoggedTransaction action_info;
        action_log.at(i, action_info);

        revert = (action_info.applied_reverted == false && i < len);

        if (revert)
        {
            ++i;
            action_stack.push(std::move(action_info));
        }
    }

    for (; i < len; ++i)
    {
        LoggedTransaction action_info;
        action_log.at(i, action_info);

        // remove all not received entries and their reverts
        if (action_info.applied_reverted == false &&
            action_info.index >= index)
            action_stack.pop();
        else
            action_stack.push(std::move(action_info));
    }

    std::stack<LoggedTransaction> reverse_stack;
    while (!action_stack.empty()) // reverse the stack
    {
        reverse_stack.push(std::move(action_stack.top()));
        action_stack.pop();
    }

    LoggedTransactions msg_actions;
    while(!reverse_stack.empty()) // list is a vector in reality :)
    {
        msg_actions.actions.push_back(std::move(reverse_stack.top()));
        reverse_stack.pop();
    }

    sk.send(peerid, msg_actions);
}

void get_hash(beltpp::packet const& packet,
              beltpp::isocket& sk,
              beltpp::isocket::peer_id const& peerid)
{
    DigestRequest msg_get_hash;
    packet.get(msg_get_hash);

    Digest msg_hash_result;
    msg_hash_result.base58_hash = meshpp::hash(msg_get_hash.package.to_string());
    msg_hash_result.package = std::move(msg_get_hash.package);

    sk.send(peerid, msg_hash_result);
}

void get_random_seed(beltpp::isocket& sk,
                     beltpp::isocket::peer_id const& peerid)
{
    meshpp::random_seed rs;
    MasterKey rs_msg;
    rs_msg.master_key = rs.get_brain_key();

    sk.send(peerid, rs_msg);
}

void get_key_pair(beltpp::packet const& packet,
                  beltpp::isocket& sk,
                  beltpp::isocket::peer_id const& peerid)
{
    KeyPairRequest kpr_msg;
    packet.get(kpr_msg);

    meshpp::random_seed rs(kpr_msg.master_key);
    meshpp::private_key pv = rs.get_private_key(kpr_msg.index);
    meshpp::public_key pb = pv.get_public_key();

    KeyPair kp_msg;
    kp_msg.master_key = rs.get_brain_key();
    kp_msg.private_key = pv.get_base58_wif();
    kp_msg.public_key = pb.to_string();
    kp_msg.index = kpr_msg.index;

    sk.send(peerid, kp_msg);
}

void get_signature(beltpp::packet const& packet,
                   beltpp::isocket& sk,
                   beltpp::isocket::peer_id const& peerid)
{
    SignRequest msg;
    packet.get(msg);

    meshpp::private_key pv(msg.private_key);
    meshpp::signature signed_msg = pv.sign(msg.package.to_string());

    Signature sg_msg;
    sg_msg.package = std::move(msg.package);
    sg_msg.signature = signed_msg.base58;
    sg_msg.public_key = pv.get_public_key().to_string();

    sk.send(peerid, sg_msg);
}

void verify_signature(beltpp::packet const& packet,
                      beltpp::isocket& sk,
                      beltpp::isocket::peer_id const& peerid)
{
    Signature msg;
    packet.get(msg);

    meshpp::signature signed_msg(msg.public_key, msg.package.to_string(), msg.signature);

    sk.send(peerid, Done());
}

void process_transfer(beltpp::packet const& package_signed_transaction,
                      beltpp::packet const& package_transfer,
                      std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl)
{
    if (package_signed_transaction.type() != SignedTransaction::rtt)
        throw std::runtime_error("Unknown object typeid to insert: " + std::to_string(package_signed_transaction.type()));

    SignedTransaction signed_transaction;
    package_signed_transaction.get(signed_transaction);

    Transfer transfer;
    package_transfer.get(transfer);

    // Expiry date check
    auto now = system_clock::now();
    system_clock::time_point delta = system_clock::from_time_t(NODES_TIME_SHIFT);
    system_clock::time_point creation = system_clock::from_time_t(signed_transaction.transaction_details.creation.tm);
    system_clock::time_point expiry = system_clock::from_time_t(signed_transaction.transaction_details.expiry.tm);

    if (now < system_clock::time_point(creation - delta))
        throw std::runtime_error("Transaction from the future!");

    if (now > expiry)
        throw std::runtime_error("Expired transaction!");

    if (chrono::duration_cast<chrono::seconds>(expiry - creation).count() > TRANSACTION_LIFETIME)
        throw std::runtime_error("Too long lifetime for transaction");

    // Authority check
    if (signed_transaction.authority != transfer.from)
        throw authority_exception(signed_transaction.authority, transfer.from);

    // Don't need store transaction if sync in process
    // and seems is too far from current block.
    // Just will check the transaction and broadcast
    if (m_pimpl->sync_headers.size() > BLOCK_TR_LENGTH)
        return;

    // Check pool
    string transfer_hash = meshpp::hash(signed_transaction.to_string());

    if (!m_pimpl->m_transaction_pool.contains(transfer_hash))
    {
        beltpp::on_failure guard([&m_pimpl] { m_pimpl->discard(); });

        // Validate and add to state
        m_pimpl->m_state.apply_transfer(transfer, signed_transaction.transaction_details.fee);

        // Add to the pool
        m_pimpl->m_transaction_pool.insert(signed_transaction);

        // Add to action log
        m_pimpl->m_action_log.log(std::move(transfer));

        m_pimpl->save(guard);
    }
}

void broadcast(beltpp::packet& package_broadcast,
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
            plog->message(self.substr(0, 2) + ", " + from.substr(0, 2) + ": " + std::to_string(direction));

        if (0 == direction)
            return;
    }

    bool chance_to_reflect = beltpp::chance_one_of(10);
    BlockchainMessage::Broadcast msg_broadcast;
    package_broadcast.get(msg_broadcast);

    if (from_rpc)
    {
        if (plog)
            plog->message("will broadcast to all");
        msg_broadcast.echoes = 2;
    }
    if (chance_to_reflect)
    {
        if (plog)
            plog->message("can reflect, 1 chance out of 10");
        if (msg_broadcast.echoes == 0)
        {
            if (plog)
                plog->message("    oh no! reflections are expired");
            chance_to_reflect = false;
        }
    }
    if (msg_broadcast.echoes > 2)
        msg_broadcast.echoes = 2;

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
        --msg_broadcast.echoes;
    }
    else if (filtered_peers.empty())
    {
        if (plog)
            plog->message("all peers will be skipped by direction");
    }

    for (auto const& peer : filtered_peers)
    {
        if (plog)
            plog->message("will rebroadcast to: " + peer.substr(0, 5));

        psk->send(peer, msg_broadcast);
    }
}



//---------------- Exceptions -----------------------
authority_exception::authority_exception(string const& _authority_provided, string const& _authority_required)
    : runtime_error("Invalid authority! authority_provided: " + _authority_provided + "  " + " authority_required: " + _authority_required)
    , authority_provided(_authority_provided)
    , authority_required(_authority_required)
{}
authority_exception::authority_exception(authority_exception const& other) noexcept
    : runtime_error(other)
    , authority_provided(other.authority_provided)
    , authority_required(other.authority_required)
{}
authority_exception& authority_exception::operator=(authority_exception const& other) noexcept
{
    dynamic_cast<runtime_error*>(this)->operator =(other);
    authority_provided = other.authority_provided;
    authority_required = other.authority_required;
    return *this;
}
authority_exception::~authority_exception() noexcept
{}

wrong_request_exception::wrong_request_exception(string const& _message)
    : runtime_error("Sxal request mi areq! message: " + _message)
    , message(_message)
{}
wrong_request_exception::wrong_request_exception(wrong_request_exception const& other) noexcept
    : runtime_error(other)
    , message(other.message)
{}
wrong_request_exception& wrong_request_exception::operator=(wrong_request_exception const& other) noexcept
{
    dynamic_cast<runtime_error*>(this)->operator =(other);
    message = other.message;
    return *this;
}
wrong_request_exception::~wrong_request_exception() noexcept
{}
