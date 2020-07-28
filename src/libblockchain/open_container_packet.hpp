#pragma once

#include "message.hpp"
#include "common.hpp"
#include "exception.hpp"
#include "transaction_handler.hpp"

#include <belt.pp/packet.hpp>

#include <mesh.pp/cryptoutility.hpp>

#include <vector>
#include <chrono>

using beltpp::packet;
using namespace BlockchainMessage;
using std::vector;
using std::string;

namespace publiqpp
{
/*
inline packet& contained_member(TaskRequest& task_request, meshpp::public_key const& pb_key)
{
    meshpp::signature signature_check(pb_key,
                                      std::to_string(task_request.task_id) +
                                      meshpp::hash(task_request.package.to_string()) +
                                      std::to_string(task_request.time_signed.tm),
                                      task_request.signature);

    system_clock::time_point current_time_point = system_clock::now();
    system_clock::time_point previous_time_point = system_clock::from_time_t(task_request.time_signed.tm);
    chrono::seconds diff_seconds = chrono::duration_cast<chrono::seconds>(current_time_point - previous_time_point);

    if(diff_seconds.count() > 3) // 3 is magic number )
        throw wrong_request_exception("Do not distrub!");

    if(task_request.package.empty())
        throw wrong_request_exception("Empty request!");

    return task_request.package;
}
*/

inline packet& contained_member(Broadcast& pck, detail::node_internals& /*impl*/)
{
    return pck.package;
}

inline packet& contained_member(SignedTransaction& signed_tx, detail::node_internals& impl)
{
    signed_transaction_validate(signed_tx,
                                std::chrono::system_clock::now(),
                                std::chrono::seconds(NODES_TIME_SHIFT),
                                impl);

    return signed_tx.transaction_details.action;
}

template <typename... Ts>
class open_container_packet;

template <typename T_container>
class open_container_packet<T_container>
{
public:
    template <typename... T_args>
    bool open(packet& pck, T_args&&... args)
    {
        if (T_container::rtt != pck.type())
            return false;

        T_container* pcontainer;
        pck.get(pcontainer);

        packet& temp = contained_member(*pcontainer, std::forward<T_args>(args)...);

        items.reserve(items.size() + 2);
        items.push_back(&pck);
        items.push_back(&temp);

        return true;
    }

    vector<packet*> items;
};

template <typename T_container, typename... Ts>
class open_container_packet<T_container, Ts...>
{
public:
    template <typename... T_args>
    bool open(packet& pck, T_args&&... args)
    {
        if (T_container::rtt != pck.type())
            return false;

        T_container* pcontainer;
        pck.get(pcontainer);

        packet& temp = contained_member(*pcontainer, std::forward<T_args>(args)...);

        open_container_packet<Ts...> sub;
        if (false == sub.open(temp, std::forward<T_args>(args)...))
            return false;

        items.reserve(items.size() + 1 + sub.items.size());
        items.push_back(&pck);
        for (auto& item : sub.items)
            items.push_back(item);

        return true;
    }

    vector<packet*> items;
};
}// end of namespace publiqpp
