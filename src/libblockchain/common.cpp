#include "common.hpp"

#include <unordered_set>

namespace publiqpp
{
namespace detail
{
void event_queue_manager::next(beltpp::event_handler& eh,
                               beltpp::stream* rpc_stream,
                               meshpp::p2psocket* p2p_stream,
                               beltpp::stream* on_demand_stream)
{
    if (false == queue.empty())
        queue.pop();

    if (queue.empty())
    {
        std::unordered_set<beltpp::event_item const*> wait_streams;

        beltpp::event_handler::wait_result wait_result = eh.wait(wait_streams);

        if (wait_result & beltpp::event_handler::event)
        {
            for (auto& wait_stream_item : wait_streams)
            {
                beltpp::socket::packets received_packets;
                beltpp::socket::peer_id peerid;
                beltpp::event_item* pevent_item = nullptr;
                if (wait_stream_item == rpc_stream)
                {
                    pevent_item = rpc_stream;
                    received_packets = rpc_stream->receive(peerid);
                }
                else if (wait_stream_item == &p2p_stream->worker())
                {
                    pevent_item = p2p_stream;
                    received_packets = p2p_stream->receive(peerid);
                }
                else
                    throw std::logic_error("wait_and_receive_one: pevent_item != rpc_stream && pevent_item != p2p_stream");

                for (auto&& reveived_packet : received_packets)
                    queue.push(stream_event::event_result(pevent_item, peerid, std::move(reveived_packet)));
            }
        }

        if (wait_result & beltpp::event_handler::timer_out)
            queue.push(stream_event::timer_result());

        if (on_demand_stream && (wait_result & beltpp::event_handler::on_demand))
        {
            beltpp::socket::packets received_packets;
            beltpp::socket::peer_id peerid;
            beltpp::event_item* pevent_item = on_demand_stream;
            received_packets = on_demand_stream->receive(peerid);

            for (auto&& reveived_packet : received_packets)
                queue.push(stream_event::event_result(pevent_item, peerid, std::move(reveived_packet)));
        }
    }
}

bool event_queue_manager::is_timer() const
{
    if (false == queue.empty() && queue.front().et == detail::stream_event::timer)
        return true;

    return false;
}

bool event_queue_manager::is_message() const
{
    if (false == queue.empty() && queue.front().et == detail::stream_event::message)
        return true;

    return false;
}

beltpp::event_item const* event_queue_manager::message_source() const
{
    if (queue.empty() || queue.front().et != detail::stream_event::message)
        throw std::logic_error("event_queue_manager::message_source: queue.empty() || queue.front().et != detail::stream_event::message");

    return queue.front().pevent_source;
}

beltpp::socket::peer_id event_queue_manager::message_peerid() const
{
    if (queue.empty() || queue.front().et != detail::stream_event::message)
        throw std::logic_error("event_queue_manager::message_source: queue.empty() || queue.front().et != detail::stream_event::message");

    return queue.front().peerid;
}


beltpp::packet& event_queue_manager::message()
{
    if (queue.empty() || queue.front().et != detail::stream_event::message)
        throw std::logic_error("event_queue_manager::message_source: queue.empty() || queue.front().et != detail::stream_event::message");

    return queue.front().package;
}

}   // end namespace detail
}   // end namespace publiqpp