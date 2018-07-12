#pragma once

#include "message.hpp"

#include <belt.pp/packet.hpp>
#include <belt.pp/scope_helper.hpp>

#include <mesh.pp/cryptoutility.hpp>

#include <vector>

using beltpp::packet;
using namespace BlockchainMessage;
using std::vector;

packet&& take_contained_object(Broadcast&& pck)
{
    return std::move(pck.package);
}
void put_contained_object_back(Broadcast& pck, packet&& value)
{
    pck.package = std::move(value);
}

packet&& take_contained_object(SignedTransaction&& pck)
{
    meshpp::public_key pb_key(pck.authority);
    auto message = detail::saver(pck.transaction_details);
    meshpp::signature signature_check(pb_key,
                                      vector<char>(message.begin(), message.end()),
                                      pck.signature);
    signature_check.check();

    return std::move(pck.transaction_details.action);
}
void put_contained_object_back(SignedTransaction& pck, packet&& value)
{
    pck.transaction_details.action = std::move(value);
}

Block&& take_contained_object(SignedBlock&& pck)
{
    meshpp::public_key pb_key(pck.authority);
    auto message = detail::saver(pck.block_details);
    meshpp::signature signature_check(pb_key,
                                      vector<char>(message.begin(), message.end()),
                                      pck.signature);
    signature_check.check();

    return std::move(pck.block_details);
}
void put_contained_object_back(SignedBlock& pck, Block&& value)
{
    pck.block_details = std::move(value);
}

template <typename... Ts>
class open_container_packet;

template <typename T_container>
class open_container_packet<T_container>
{
public:
    bool open(packet&& pck, vector<packet*>& composition)
    {
        if (T_container::rtt != pck.type())
            return false;

        T_container container;
        std::move(pck).get(container);
        beltpp::on_failure check1([&pck, &container]
        {
            pck.set(std::move(container));
        });

        auto&& temp = take_contained_object(std::move(container));
        beltpp::on_failure check2([&container, &temp]
        {
            put_contained_object_back(container, std::move(temp));
        });

        items.reserve(items.size() + 2);
        composition.clear();
        composition.reserve(items.size());
        items.push_back(std::move(container));
        items.push_back(std::move(temp));

        for (auto& item : items)
            composition.push_back(&item);

        check2.dismiss();
        check1.dismiss();

        return true;
    }

    vector<packet> items;
};
template <typename T_container, typename... Ts>
class open_container_packet<T_container, Ts...>
{
public:
    bool open(packet&& pck, vector<packet*>& composition)
    {
        if (T_container::rtt != pck.type())
            return false;

        T_container container;
        std::move(pck).get(container);
        beltpp::on_failure check1([&pck, &container]
        {
            pck.set(std::move(container));
        });

        packet temp;
        temp = take_contained_object(std::move(container));
        beltpp::on_failure check2([&container, &temp]
        {
            put_contained_object_back(container, std::move(temp));
        });

        open_container_packet<Ts...> sub;
        if (false == sub.open(std::move(temp), composition))
            return false;

        items.reserve(items.size() + 1 + sub.items.size());
        composition.clear();
        composition.reserve(items.size());
        items.push_back(std::move(container));
        for (auto& item : sub.items)
            items.push_back(std::move(item));

        for (auto& item : items)
            composition.push_back(&item);

        check1.dismiss();
        check2.dismiss();

        return true;
    }

    vector<packet> items;
};
