#include "common.hpp"

#include <unordered_set>

namespace publiqpp
{
namespace detail
{
wait_result_item wait_and_receive_one(wait_result& wait_result_info,
                                      beltpp::event_handler& eh,
                                      beltpp::stream* rpc_stream,
                                      meshpp::p2psocket* p2p_stream,
                                      beltpp::stream* on_demand_stream)
{
    auto& info = wait_result_info.m_wait_result;

    if (info == beltpp::event_handler::wait_result::nothing)
    {
        assert(wait_result_info.event_packets.empty());
        if (false == wait_result_info.event_packets.empty())
            throw std::logic_error("false == wait_result_info.event_packets.empty()");

        std::unordered_set<beltpp::event_item const*> wait_streams;

        info = eh.wait(wait_streams);

        if (info & beltpp::event_handler::event)
        {
            for (auto& pevent_item : wait_streams)
            {
                beltpp::socket::packets received_packets;
                beltpp::socket::peer_id peerid;
                if (pevent_item == rpc_stream)
                    received_packets = rpc_stream->receive(peerid);
                else if (pevent_item == &p2p_stream->worker())
                    received_packets = p2p_stream->receive(peerid);
                else
                    throw std::logic_error("wait_and_receive_one: pevent_item != rpc_stream && pevent_item != p2p_stream");

                if (false == received_packets.empty())
                    wait_result_info.event_packets[pevent_item] = std::make_pair(peerid, std::move(received_packets));
            }
        }

        /*if (wait_result & beltpp::event_handler::timer_out)
        {
        }*/

        if (on_demand_stream && (info & beltpp::event_handler::on_demand))
        {
            beltpp::socket::packets received_packets;
            beltpp::socket::peer_id peerid;
            received_packets = on_demand_stream->receive(peerid);

            if (false == received_packets.empty())
                wait_result_info.event_packets[on_demand_stream] = std::make_pair(peerid, std::move(received_packets));
        }
    }

    auto find_event = [](wait_result::event_results& container, beltpp::stream* on_demand_stream, bool get_match)
                        {
                            auto it = container.begin();
                            for (; it != container.end(); ++it)
                            {
                                if (get_match && it->first == on_demand_stream)
                                    break;
                                if (false == get_match && it->first != on_demand_stream)
                                    break;
                            }

                            return it;
                        };

    auto result = wait_result_item::empty_result();

    if (info & beltpp::event_handler::event)
    {
        auto event_item_it = find_event(wait_result_info.event_packets, on_demand_stream, false);

        if (event_item_it != wait_result_info.event_packets.end())
        {
            wait_result::packets_result& packets_result = event_item_it->second;

            if (false == packets_result.second.empty())
            {
                auto packet = std::move(packets_result.second.front());
                auto peerid = packets_result.first;
                auto pevent_item = event_item_it->first;

                packets_result.second.pop_front();

                result = wait_result_item::event_result(pevent_item, peerid, std::move(packet));
            }

            if (packets_result.second.empty())
                wait_result_info.event_packets.erase(event_item_it);
        }

        auto event_item_it_again = find_event(wait_result_info.event_packets, on_demand_stream, false);
        if (event_item_it_again == wait_result_info.event_packets.end())
            info = beltpp::event_handler::wait_result(info & ~beltpp::event_handler::event);

        return result;
    }

    if (info & beltpp::event_handler::timer_out)
    {
        info = beltpp::event_handler::wait_result(info & ~beltpp::event_handler::timer_out);
        result = wait_result_item::timer_result();
        return result;
    }

    if (info & beltpp::event_handler::on_demand)
    {
        auto on_demand_item_it = find_event(wait_result_info.event_packets, on_demand_stream, true);

        if (on_demand_item_it != wait_result_info.event_packets.end())
        {
            wait_result::packets_result& packets_result = on_demand_item_it->second;

            if (false == packets_result.second.empty())
            {
                auto packet = std::move(packets_result.second.front());
                auto peerid = packets_result.first;
                auto pevent_item = on_demand_item_it->first;

                packets_result.second.pop_front();

                result = wait_result_item::event_result(pevent_item, peerid, std::move(packet));
            }

            if (packets_result.second.empty())
                wait_result_info.event_packets.erase(on_demand_item_it);
        }

        auto on_demand_item_it_again = find_event(wait_result_info.event_packets, on_demand_stream, true);
        if (on_demand_item_it_again == wait_result_info.event_packets.end())
            info = beltpp::event_handler::wait_result(info & ~beltpp::event_handler::on_demand);

        return result;
    }

    return result;
}
}   // end namespace detail
}   // end namespace publiqpp