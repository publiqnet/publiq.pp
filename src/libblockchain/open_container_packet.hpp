#pragma once

#include "message.hpp"

#include <belt.pp/packet.hpp>
#include <belt.pp/scope_helper.hpp>

#include <mesh.pp/cryptoutility.hpp>

#include <vector>

using beltpp::packet;
using namespace BlockchainMessage;
using std::vector;

packet& contained_member(Broadcast& pck)
{
    return pck.package;
}

packet& contained_member(SignedTransaction& pck)
{
    meshpp::public_key pb_key(pck.authority);
    auto message = detail::saver(pck.transaction_details);
    meshpp::signature signature_check(pb_key,
                                      vector<char>(message.begin(), message.end()),
                                      pck.signature);
    signature_check.check();

    return pck.transaction_details.action;
}

template <typename... Ts>
class open_container_packet;

template <typename T_container>
class open_container_packet<T_container>
{
public:
    bool open(packet& pck, vector<packet*>& composition)
    {
        if (T_container::rtt != pck.type())
            return false;

        T_container* pcontainer;
        pck.get(pcontainer);

        packet& temp = contained_member(*pcontainer);

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
    bool open(packet& pck, vector<packet*>& composition)
    {
        if (T_container::rtt != pck.type())
            return false;

        T_container* pcontainer;
        pck.get(pcontainer);

        packet& temp = contained_member(*pcontainer);

        open_container_packet<Ts...> sub;
        if (false == sub.open(temp, composition))
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
