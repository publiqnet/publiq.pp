#include "communication_rpc.hpp"
#include "message.hpp"

#include <mesh.pp/cryptoutility.hpp>

#include <stack>
#include <vector>

using std::vector;
using std::stack;

using namespace BlockchainMessage;

void submit_actions(beltpp::packet const& packet,
                    publiqpp::state& state,
                    beltpp::isocket& sk,
                    beltpp::isocket::peer_id const& peerid)
{
    bool answered = false;
    try
    {
        SubmitActions submitactions_msg;
        packet.get(submitactions_msg);

        switch (submitactions_msg.item.type())
        {
        case Reward::rtt: //  check reward for testing
        case Transfer::rtt: // check transaction for testing
        case NewArticle::rtt:
        {
            state.action_log().insert(submitactions_msg.item);
            sk.send(peerid, Done());
            answered = true;
            break;
        }
        case RevertLastAction::rtt: //  pay attention - RevertLastAction is sent, but RevertActionAt is stored
        {
            // check if last action is revert
            int revert_mark = 0;
            size_t index = state.action_log().length() - 1;
            bool revert = true;

            while (revert)
            {
                beltpp::packet packet;
                state.action_log().at(index, packet);

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
            state.action_log().at(index, packet);

            RevertActionAt msg_revert;
            msg_revert.index = index;
            msg_revert.item = std::move(packet);

            state.action_log().insert(msg_revert);
            sk.send(peerid, Done());
            answered = true;
            break;
        }
        default:
            throw std::runtime_error("Unsupported action!");
            break;
        }
    }
    catch (std::exception const& ex)
    {
        if (false == answered)
        {
            Failed msg_failed;
            msg_failed.message = ex.what();
            sk.send(peerid, msg_failed);
        }
    }
    catch (...)
    {
        if (false == answered)
        {
            Failed msg_failed;
            msg_failed.message = "unknown exception";
            sk.send(peerid, msg_failed);
        }
    }
}

void get_actions(beltpp::packet const& packet,
                 publiqpp::state& state,
                 beltpp::isocket& sk,
                 beltpp::isocket::peer_id const& peerid)
{
    bool answered = false;

    try
    {
        GetActions msg_get_actions;
        packet.get(msg_get_actions);
        uint64_t index = msg_get_actions.start_index;

        stack<Action> action_stack;

        size_t i = index;
        size_t len = state.action_log().length();

        bool revert = i < len;
        while (revert) //the case when next action is revert
        {
            beltpp::packet packet;
            state.action_log().at(i, packet);

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
            state.action_log().at(i, packet);

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
        while (!action_stack.empty()) // revers the stack
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
        answered = true;
    }
    catch (std::exception const& ex)
    {
        if (false == answered)
        {
            Failed msg_failed;
            msg_failed.message = ex.what();
            sk.send(peerid, msg_failed);
        }
    }
    catch (...)
    {
        if (false == answered)
        {
            Failed msg_failed;
            msg_failed.message = "unknown exception";
            sk.send(peerid, msg_failed);
        }
    }
}



void get_hash(beltpp::packet const& packet,
              publiqpp::state& state,
              beltpp::isocket& sk,
              beltpp::isocket::peer_id const& peerid)
{
    bool answered = false;

    try
    {
        GetHash msg_get_hash;
        packet.get(msg_get_hash);

        vector<char> buffer = msg_get_hash.item.save();

        HashResult msg_hash_result;
        msg_hash_result.base58_hash = meshpp::hash(buffer.begin(), buffer.end());
        msg_hash_result.item = std::move(msg_get_hash.item);

        sk.send(peerid, msg_hash_result);
        answered = true;
    }
    catch (std::exception const& ex)
    {
        if (false == answered)
        {
            Failed msg_failed;
            msg_failed.message = ex.what();
            sk.send(peerid, msg_failed);
        }
    }
    catch (...)
    {
        if (false == answered)
        {
            Failed msg_failed;
            msg_failed.message = "unknown exception";
            sk.send(peerid, msg_failed);
        }
    }
}

