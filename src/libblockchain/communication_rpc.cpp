#include "communication_rpc.hpp"
#include "message.hpp"

#include <mesh.pp/cryptoutility.hpp>

#include <stack>
#include <vector>

using std::vector;
using std::stack;
using std::string;

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

void revert_last_action(publiqpp::action_log& action_log,
                        beltpp::isocket& sk,
                        beltpp::isocket::peer_id const& peerid)
 {
     int revert_mark = 0;
     size_t index = action_log.length() - 1;
     bool revert = true;

     while (revert)
     {
         LoggedTransaction action_info;
         action_log.at(index, action_info);

         revert = (action_info.applied_reverted == false);

         if (revert)
             ++revert_mark;
         else
             --revert_mark;

         if (revert_mark >= 0)
         {
             if (index == 0)
                 throw std::runtime_error("Nothing to revert!");

             --index;
             revert = true;
         }
     }

     // revert last valid action
     LoggedTransaction action_revert_info;
     action_log.at(index, action_revert_info);

     action_revert_info.applied_reverted = false;   //  revert

     action_log.insert(action_revert_info);
     sk.send(peerid, Done());
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

void process_transfer(beltpp::packet const& packet,
                      publiqpp::action_log& action_log,
                      publiqpp::transaction_pool& transaction_pool,
                      publiqpp::state& state,
                      beltpp::isocket& sk,
                      beltpp::isocket::peer_id const& peerid)
{
    SignedTransaction signed_transaction;
    packet.get(signed_transaction);

    Transfer transfer;
    signed_transaction.action_details.action.get(transfer);

    // Quick validate
    if (signed_transaction.authority != transfer.from)
    {
        InvalidAuthority msg;
        msg.authority_abuser = signed_transaction.authority;
        msg.authority_abused = transfer.from;
        sk.send(peerid, msg);
        return;
    }

    // Check pool
    vector<char> packet_vec = packet.save();
    string packet_hash = meshpp::hash(packet_vec.begin(), packet_vec.end());

    if (transaction_pool.contains(packet_hash))
        throw std::runtime_error("Transaction is already received!");

    // Validate state
    if (!state.possible_transfer(transfer, signed_transaction.action_details.fee))
        throw std::runtime_error("Balance is not enough!");

    // Add to the pool
    transaction_pool.insert(packet);

    // Apply state
    state.apply_transfer(transfer, signed_transaction.action_details.fee);

    // Add to action log
    LoggedTransaction action_info;
    action_info.applied_reverted = false;    //  apply
    action_info.index = 0; // will be set automatically
    action_info.action = std::move(signed_transaction.action_details);

    action_log.insert(action_info);

    // Boradcast

    sk.send(peerid, Done());
}

