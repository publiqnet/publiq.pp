#include "communication_rpc.hpp"
#include "message.hpp"

#include <mesh.pp/cryptoutility.hpp>

#include <stack>
#include <vector>
#include <chrono>

using std::vector;
using std::stack;
using std::string;
namespace chrono = std::chrono;
using system_clock = chrono::system_clock;

using namespace BlockchainMessage;

void submit_action(beltpp::packet&& package,
                   publiqpp::action_log& action_log,
                   publiqpp::transaction_pool& transaction_pool,
                   beltpp::isocket& sk,
                   beltpp::isocket::peer_id const& peerid)
{
    LogTransaction log_transaction_msg;
    std::move(package).get(log_transaction_msg);

    auto& ref_action = log_transaction_msg.action;
    switch (ref_action.type())
    {
    case Reward::rtt: //  check reward for testing
    case Transfer::rtt: // check transaction for testing
    //case NewArticle::rtt:
    {
        if (Reward::rtt == ref_action.type())
        {
            Reward msg_reward;
            ref_action.get(msg_reward);
            // following will throw on invalid public key
            meshpp::public_key temp(msg_reward.to);
        }
        else if (Transfer::rtt == ref_action.type())
        {
            Transfer msg_transfer;
            ref_action.get(msg_transfer);
            // following will throw on invalid public key
            //meshpp::public_key temp1(msg_transfer.to);
            //meshpp::public_key temp2(msg_transfer.from);

            transaction_pool.insert(msg_transfer);
        }

        LoggedTransaction action_info;
        action_info.applied_reverted = true;    //  apply
        action_info.index = 0; // will be set automatically
        action_info.action = std::move(ref_action);

        action_log.insert(action_info);
        break;
    }
    default:
        throw std::runtime_error("Unsupported action!");
        break;
    }
}

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
    HashRequest msg_get_hash;
    packet.get(msg_get_hash);

    vector<char> buffer = msg_get_hash.package.save();

    HashResult msg_hash_result;
    msg_hash_result.base58_hash = meshpp::hash(buffer.begin(), buffer.end());
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
    meshpp::signature signed_msg = pv.sign(msg.package.save());

    Signature sg_msg;
    sg_msg.package = std::move(msg.package);
    sg_msg.signature = signed_msg.base64;
    sg_msg.public_key = pv.get_public_key().to_string();

    sk.send(peerid, sg_msg);
}

void verify_signature(beltpp::packet const& packet,
                      beltpp::isocket& sk,
                      beltpp::isocket::peer_id const& peerid)
{
    Signature msg;
    packet.get(msg);

    meshpp::signature signed_msg(msg.public_key, msg.package.save(), msg.signature);
    signed_msg.check();

    sk.send(peerid, Done());
}

void process_transfer(beltpp::packet const& package_signed_transaction,
                      beltpp::packet const& package_transfer,
                      publiqpp::action_log& action_log,
                      publiqpp::transaction_pool& transaction_pool,
                      publiqpp::state& state)
{
    SignedTransaction signed_transaction;
    package_signed_transaction.get(signed_transaction);

    Transfer transfer;
    package_transfer.get(transfer);

    // Expiry date check
    auto now = system_clock::now();
    system_clock::to_time_t(now);

    if (now <= system_clock::from_time_t(signed_transaction.transaction_details.expiry.tm))
        throw std::runtime_error("Expired transaction!");

    // Authority check
    if (signed_transaction.authority != transfer.from)
        throw exception_authority(signed_transaction.authority, transfer.from);

    // Check pool
    vector<char> packet_vec = package_transfer.save();
    string transfer_hash = meshpp::hash(packet_vec.begin(), packet_vec.end());

    if (false == transaction_pool.contains(transfer_hash))
    {
        // Validate state
        if (!state.check_transfer(transfer, signed_transaction.transaction_details.fee))
            throw std::runtime_error("Balance is not enough!");

        // Add to the pool
        transaction_pool.insert(signed_transaction.transaction_details.action);

        // Apply state
        state.apply_transfer(transfer, signed_transaction.transaction_details.fee);

        // Add to action log
        LoggedTransaction action_info;
        action_info.applied_reverted = true;    //  apply
        action_info.index = 0; // will be set automatically
        action_info.action = std::move(transfer);

        action_log.insert(action_info);
    }
}

void insert_blocks(std::vector<beltpp::packet>& package_blocks,
                   publiqpp::action_log& action_log,
                   publiqpp::transaction_pool& transaction_pool,
                   publiqpp::state& state)
{
    std::unordered_set<string> used_key_set;
    std::unordered_set<string> erase_tpool_set;
    std::unordered_map<string, uint64_t> tmp_state;
    std::vector<LoggedTransaction> logged_transactions;
    std::vector<std::pair<std::string, uint64_t>> amounts;

    auto it = package_blocks.begin();
    for (; it != package_blocks.end(); ++it)
    {
        Block block;
        (*it).get(block);

        // Check block transactions and calculate new state
        for (auto &signed_transaction : block.signed_transactions)
        {
            // Verify signed_transaction signature
            vector<char> buffer = SignedTransaction::saver(&signed_transaction.transaction_details);
            meshpp::signature sg(meshpp::public_key(signed_transaction.authority), buffer, signed_transaction.signature);

            sg.check();

            beltpp::packet package_transaction = std::move(signed_transaction.transaction_details.action);
            vector<char> packet_vec = package_transaction.save();
            string transfer_hash = meshpp::hash(packet_vec.begin(), packet_vec.end());

            string key;
            uint64_t amount;

            Transaction transaction;
            std::move(package_transaction).get(transaction);

            beltpp::packet package_transfer = std::move(transaction.action);

            Transfer transfer;
            std::move(package_transfer).get(transfer);

            if (signed_transaction.authority != transfer.from)
                throw exception_authority(signed_transaction.authority, transfer.from);

            // correct "from" key balance
            key = transfer.from;
            tmp_state[key] = state.get_balance(key);

            if (used_key_set.find(key) == used_key_set.end())
            {
                used_key_set.insert(key);

                // process "key" output transfers
                amounts.clear();
                transaction_pool.get_amounts(key, amounts, false);

                amount = tmp_state[key];
                for (auto& it : amounts)
                {
                    amount += it.second;
                    erase_tpool_set.insert(it.first);
                }

                // process "key" input transfers
                amounts.clear();
                transaction_pool.get_amounts(key, amounts, true);

                for (auto& it : amounts)
                {
                    if (amount >= it.second)
                    {
                        amount -= it.second;
                        erase_tpool_set.insert(it.first);
                    }
                    else
                        throw exception_balance(key);
                }
                tmp_state[key] = amount;
            }

            // remove transfer amount and fee from sender balance
            amount = tmp_state[key];
            if (amount >= transfer.amount + transaction.fee)
                tmp_state[key] = amount - transfer.amount - transaction.fee;
            else
                throw exception_balance(key);

            // correct to_key balance
            key = transfer.to;
            tmp_state[key] = state.get_balance(key);

            if (used_key_set.find(key) == used_key_set.end())
            {
                used_key_set.insert(key);

                // process "key" output transfers
                amounts.clear();
                transaction_pool.get_amounts(key, amounts, false);

                amount = tmp_state[key];
                for (auto& it : amounts)
                {
                    amount += it.second;
                    erase_tpool_set.insert(it.first);
                }

                // process "key" input transfers
                amounts.clear();
                transaction_pool.get_amounts(key, amounts, true);

                for (auto& it : amounts)
                {
                    if (amount >= it.second)
                    {
                        amount -= it.second;
                        erase_tpool_set.insert(it.first);
                    }
                    else
                        throw exception_balance(key);
                }
                tmp_state[key] = amount;
            }

            // add transfer amount to receiver balance
            amount = tmp_state[key];
            tmp_state[key] = amount + transfer.amount;

            // collect action log
            LoggedTransaction action_info;
            action_info.applied_reverted = true;
            action_info.index = 0;
            action_info.action = std::move(transaction);
            logged_transactions.push_back(std::move(action_info));
        }
    }

    // Correct state
    state.merge_block(tmp_state);

    // Correct action log ( 1. revert old action log )
    size_t count = transaction_pool.length();
    for (size_t i = 0; i < count; ++i)
        action_log.revert();

    // Correct action log ( 2. apply block transfers )
    for (auto &it : logged_transactions)
        action_log.insert(it);

    // Correct action log ( 3. apply rest of transaction pool )
    for (auto &it : erase_tpool_set)
        transaction_pool.remove(it);

    std::vector<std::string> keys;
    transaction_pool.get_keys(keys);

    auto now = system_clock::now();
    system_clock::to_time_t(now);

    for (auto &it : keys)
    {
        beltpp::packet packet;
        transaction_pool.at(it, packet);

        Transaction transaction;
        packet.get(transaction);

        if (now > system_clock::from_time_t(transaction.expiry.tm))
        {
            LoggedTransaction action_info;
            action_info.applied_reverted = true;
            action_info.index = 0;
            action_info.action = std::move(packet);

            action_log.insert(action_info);
        }
    }
}

void revert_blocks(size_t count,
                   publiqpp::blockchain& blockchain,
                   publiqpp::transaction_pool& transaction_pool)
{
    for (size_t i = 0; i < count; ++i)
    {
        Block block;
        blockchain.get_last_block(block);

        auto it = block.signed_transactions.rbegin();

        // Add block transactions to the pool
        for (; it != block.signed_transactions.rend(); ++it)
            transaction_pool.insert((*it).transaction_details.action);

        // Remove last block from blockchain
        blockchain.remove_last_block();
    }

    // State shoul be correct :)

    // Action log should be correct :)
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

    uint64_t echoes = msg_broadcast.echoes;
    if (from_rpc)
    {
        if (plog)
            plog->message("will broadcast to all");
        echoes = 2;
    }
    if (chance_to_reflect)
    {
        if (plog)
            plog->message("can reflect, 1 chance out of 10");
        if (msg_broadcast.echoes == 0)
        {
            if (plog)
                plog->message("    oh no!");
            chance_to_reflect = false;
        }
    }

    for (auto const& peer : all_peers)
    {
        bool do_reflect = false;
        auto direction2 = str_compare(self, peer);
        if (false == from_rpc &&
            direction == direction2)
        {
            if (false == chance_to_reflect)
            {
                if (plog)
                    plog->message("skip: " + self.substr(0,2) + ", " + peer.substr(0,2) + ": " + std::to_string(direction2));
                continue;
            }
            else
                do_reflect = true;
        }

        msg_broadcast.echoes = echoes;
        if (do_reflect)
        {
            if (plog)
            {
                plog->message("will reflect broadcast to: " + peer.substr(0, 5));
                plog->message("    " + std::to_string(msg_broadcast.echoes));
            }
            --msg_broadcast.echoes;
        }
        else if (plog)
            plog->message("will rebroadcast to: " + peer.substr(0, 5));

        psk->send(peer, msg_broadcast);
    }
}



//---------------- Exceptions -----------------------
exception_authority::exception_authority(string const& _authority_provided, string const& _authority_required)
    : runtime_error("Invalid authority! authority_provided: " + _authority_provided + "  " + " authority_required: " + _authority_required)
    , authority_provided(_authority_provided)
    , authority_required(_authority_required)
{}
exception_authority::exception_authority(exception_authority const& other) noexcept
    : runtime_error(other)
    , authority_provided(other.authority_provided)
    , authority_required(other.authority_required)
{}
exception_authority& exception_authority::operator=(exception_authority const& other) noexcept
{
    dynamic_cast<runtime_error*>(this)->operator =(other);
    authority_provided = other.authority_provided;
    authority_required = other.authority_required;
    return *this;
}
exception_authority::~exception_authority() noexcept
{}

exception_balance::exception_balance(string const& _account)
    : runtime_error("Low balance! account: " + account)
    , account(_account)
{}
exception_balance::exception_balance(exception_balance const& other) noexcept
    : runtime_error(other)
    , account(other.account)
{}
exception_balance& exception_balance::operator=(exception_balance const& other) noexcept
{
    dynamic_cast<runtime_error*>(this)->operator =(other);
    account = other.account;
    return *this;
}
exception_balance::~exception_balance() noexcept
{}
