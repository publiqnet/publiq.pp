#include "common.hpp"

#include <unordered_set>

namespace publiqpp
{
namespace detail
{
stream_event wait_and_receive_one(event_queue_manager& event_queue,
                                  beltpp::event_handler& eh,
                                  beltpp::stream* rpc_stream,
                                  meshpp::p2psocket* p2p_stream,
                                  beltpp::stream* on_demand_stream)
{
    if (event_queue.queue.empty())
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
                    event_queue.queue.push(stream_event::event_result(pevent_item, peerid, std::move(reveived_packet)));
            }
        }

        if (wait_result & beltpp::event_handler::timer_out)
            event_queue.queue.push(stream_event::timer_result());

        if (on_demand_stream && (wait_result & beltpp::event_handler::on_demand))
        {
            beltpp::socket::packets received_packets;
            beltpp::socket::peer_id peerid;
            beltpp::event_item* pevent_item = on_demand_stream;
            received_packets = on_demand_stream->receive(peerid);

            for (auto&& reveived_packet : received_packets)
                event_queue.queue.push(stream_event::event_result(pevent_item, peerid, std::move(reveived_packet)));
        }
    }

    if (false == event_queue.queue.empty())
    {
        auto result = std::move(event_queue.queue.front());
        event_queue.queue.pop();

        return result;
    }

    return stream_event::empty_result();
}
}   // end namespace detail
}   // end namespace publiqpp