#include "message.hpp"
#include "http.hpp"

#include <publiq.pp/message.hpp>

#include <belt.pp/socket.hpp>

#include <boost/filesystem.hpp>

#include <iostream>
#include <chrono>
#include <thread>
#include <memory>
#include <chrono>
#include <unordered_set>

using peer_id = beltpp::socket::peer_id;

using std::cout;
using std::endl;
using std::unique_ptr;
namespace chrono = std::chrono;
using std::unordered_set;
using beltpp::packet;

using sf = beltpp::socket_family_t<&commander::http::message_list_load<&CommanderMessage::message_list_load>>;

using namespace CommanderMessage;

int main(int argc, char** argv)
{
    unique_ptr<beltpp::event_handler> m_ptr_eh(new beltpp::event_handler());
    unique_ptr<beltpp::socket> m_ptr_rpc_socket(new beltpp::socket(
                                   beltpp::getsocket<sf>(*m_ptr_eh)
                                   ));

    m_ptr_eh->set_timer(chrono::seconds(10));
    m_ptr_eh->add(*m_ptr_rpc_socket);

    m_ptr_rpc_socket->listen(beltpp::ip_address("0.0.0.0", 8080));

    while (true)
    {
        unordered_set<beltpp::ievent_item const*> wait_sockets;
        auto wait_result = m_ptr_eh->wait(wait_sockets);

        if (wait_result == beltpp::event_handler::event)
        {
            for (auto& pevent_item : wait_sockets)
            {
                beltpp::isocket* psk = nullptr;
                if (pevent_item == m_ptr_rpc_socket.get())
                    psk = m_ptr_rpc_socket.get();

                beltpp::isocket::peer_id peerid;
                beltpp::socket::packets received_packets;
                if (psk != nullptr)
                    received_packets = psk->receive(peerid);

                for (auto& received_packet : received_packets)
                {
                try
                {
                    packet& ref_packet = received_packet;

                    switch (ref_packet.type())
                    {
                    case UsersRequest::rtt:
                        UsersResponse msg;
                        msg.users.push_back("user1");
                        msg.users.push_back("user2");

                        psk->send(peerid, msg);
                        break;
                    }
                }
                catch(...)
                {}
                }
            }
        }
        else
        {

        }
    }

    return 0;
}
