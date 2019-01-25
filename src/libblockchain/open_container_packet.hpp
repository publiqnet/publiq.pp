#pragma once

#include "message.hpp"
#include "common.hpp"
#include "exception.hpp"

#include <belt.pp/packet.hpp>

#include <mesh.pp/cryptoutility.hpp>

#include <vector>
#include <chrono>

using beltpp::packet;
using namespace BlockchainMessage;
using std::vector;
using std::string;

inline packet& contained_member(TaskRequest& task_request, meshpp::public_key& pb_key)
{
    meshpp::signature signature_check(pb_key,
                                      std::to_string(task_request.task_id) + task_request.package.to_string(),
                                      task_request.signature);

    return task_request.package;
}

inline packet& contained_member(Broadcast& pck)
{
    return pck.package;
}

inline packet& contained_member(SignedTransaction& signed_tx)
{
    meshpp::public_key pb_key(signed_tx.authority);
    meshpp::signature signature_check(pb_key,
                                      signed_tx.transaction_details.to_string(),
                                      signed_tx.signature);

    namespace chrono = std::chrono;
    using chrono::system_clock;
    using time_point = system_clock::time_point;
    time_point creation =
            system_clock::from_time_t(signed_tx.transaction_details.creation.tm);
    time_point expiry =
            system_clock::from_time_t(signed_tx.transaction_details.expiry.tm);

    // Expiry date check
    auto now = system_clock::now();

    if (now < creation - chrono::seconds(NODES_TIME_SHIFT))
        throw wrong_data_exception("Transaction from the future!");

    if (now > expiry)
        throw wrong_data_exception("Expired transaction!");

    if (expiry - creation > std::chrono::hours(TRANSACTION_MAX_LIFETIME_HOURS))
        throw wrong_data_exception("Too long lifetime for transaction");

    return signed_tx.transaction_details.action;
}

template <typename... Ts>
class open_container_packet;

template <typename T_container>
class open_container_packet<T_container>
{
public:
    template <typename... T_args>
    bool open(packet& pck, vector<packet*>& composition, T_args&&... args)
    {
        if (T_container::rtt != pck.type())
            return false;

        T_container* pcontainer;
        pck.get(pcontainer);

        packet& temp = contained_member(*pcontainer, std::forward<T_args>(args)...);

        items.reserve(items.size() + 2);
        items.push_back(&pck);
        items.push_back(&temp);

        composition = items;

        return true;
    }

    vector<packet*> items;
};
template <typename T_container, typename... Ts>
class open_container_packet<T_container, Ts...>
{
public:
    template <typename... T_args>
    bool open(packet& pck, vector<packet*>& composition, T_args&&... args)
    {
        if (T_container::rtt != pck.type())
            return false;

        T_container* pcontainer;
        pck.get(pcontainer);

        packet& temp = contained_member(*pcontainer, std::forward<T_args>(args)...);

        open_container_packet<Ts...> sub;
        if (false == sub.open(temp, composition, std::forward<T_args>(args)...))
            return false;

        items.reserve(items.size() + 1 + sub.items.size());
        items.push_back(&pck);
        for (auto& item : sub.items)
            items.push_back(item);

        composition = items;

        return true;
    }

    vector<packet*> items;
};
