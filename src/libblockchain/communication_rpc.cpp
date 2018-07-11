#include "communication_rpc.hpp"
#include "message.hpp"

#include <mesh.pp/cryptoutility.hpp>

#include <stack>
#include <vector>

using std::vector;
using std::stack;
using std::string;

using namespace BlockchainMessage;

void submit_actions(beltpp::packet const& packet,
                    publiqpp::action_log& action_log,
                    publiqpp::transaction_pool& transaction_pool)
{
    SubmitActions submitactions_msg;
    packet.get(submitactions_msg);

    switch (submitactions_msg.item.type())
    {
    case Reward::rtt: //  check reward for testing
    case Transfer::rtt: // check transaction for testing
    case NewArticle::rtt:
    {
        if (Reward::rtt == submitactions_msg.item.type())
        {
            Reward msg_reward;
            submitactions_msg.item.get(msg_reward);
            // following will throw on invalid public key
            meshpp::public_key temp(msg_reward.to.public_key);
        }
        else if (Transfer::rtt == submitactions_msg.item.type())
        {
            Transfer msg_transfer;
            submitactions_msg.item.get(msg_transfer);
            // following will throw on invalid public key
            //meshpp::public_key temp1(msg_transfer.to.public_key);
            //meshpp::public_key temp2(msg_transfer.from.public_key);

            transaction_pool.insert(msg_transfer);
        }
        action_log.insert(submitactions_msg.item);
        break;
    }
    case RevertLastAction::rtt: //  pay attention - RevertLastAction is sent, but RevertActionAt is stored
    {
        // check if last action is revert
        int revert_mark = 0;
        size_t index = action_log.length() - 1;
        bool revert = true;

        while (revert)
        {
            beltpp::packet packet;
            action_log.at(index, packet);

            revert = packet.type() == RevertActionAt::rtt;

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
        beltpp::packet packet;
        action_log.at(index, packet);

        RevertActionAt msg_revert;
        msg_revert.index = index;
        msg_revert.item = std::move(packet);

        action_log.insert(msg_revert);
        break;
    }
    default:
        throw std::runtime_error("Unsupported action!");
        break;
    }
}

void get_actions(beltpp::packet const& packet,
                 publiqpp::action_log& action_log,
                 beltpp::isocket& sk,
                 beltpp::isocket::peer_id const& peerid)
{
    GetActions msg_get_actions;
    packet.get(msg_get_actions);
    uint64_t index = msg_get_actions.start_index;

    stack<Action> action_stack;

    size_t i = index;
    size_t len = action_log.length();

    bool revert = i < len;
    while (revert) //the case when next action is revert
    {
        beltpp::packet packet;
        action_log.at(i, packet);

        revert = packet.type() == RevertActionAt::rtt;

        if (revert)
        {
            ++i;
            Action action;
            action.index = i;
            action.item = std::move(packet);

            action_stack.push(action);
        }
    }

    for (; i < len; ++i)
    {
        beltpp::packet packet;
        action_log.at(i, packet);

        // remove all not received entries and their reverts
        revert = packet.type() == RevertActionAt::rtt;
        if (revert)
        {
            RevertActionAt msg;
            packet.get(msg);

            revert = msg.index >= index;
        }

        if (revert)
        {
            action_stack.pop();
        }
        else
        {
            Action action;
            action.index = i;
            action.item = std::move(packet);

            action_stack.push(action);
        }
    }

    std::stack<Action> reverse_stack;
    while (!action_stack.empty()) // reverse the stack
    {
        reverse_stack.push(action_stack.top());
        action_stack.pop();
    }

    Actions msg_actions;
    while(!reverse_stack.empty()) // list is a vector in reality :)
    {
        msg_actions.list.push_back(std::move(reverse_stack.top()));
        reverse_stack.pop();
    }

    sk.send(peerid, msg_actions);
}

void get_hash(beltpp::packet const& packet,
              beltpp::isocket& sk,
              beltpp::isocket::peer_id const& peerid)
{
    GetHash msg_get_hash;
    packet.get(msg_get_hash);

    vector<char> buffer = msg_get_hash.item.save();

    HashResult msg_hash_result;
    msg_hash_result.base58_hash = meshpp::hash(buffer.begin(), buffer.end());
    msg_hash_result.item = std::move(msg_get_hash.item);

    sk.send(peerid, msg_hash_result);
}

void process_transfer(beltpp::packet const& packet,
                      publiqpp::action_log& action_log,
                      publiqpp::transaction_pool& transaction_pool,
                      publiqpp::state& state)
{
    Transfer transfer;
    packet.get(transfer);

    // Check pool
    vector<char> packet_vec = packet.save();
    string packet_hash = meshpp::hash(packet_vec.begin(), packet_vec.end());

    if (transaction_pool.contains(packet_hash))
        throw std::runtime_error("Transaction is already received!");

    // Validate state
    if (!state.possible_transfer(transfer))//fee
        throw std::runtime_error("Balance is not enough!");

    // Add to the pool
    transaction_pool.insert(packet);

    // Apply state
    state.apply_transfer(transfer);//fee

    // Add to action log
    action_log.insert(packet);

    // Boradcast
}

