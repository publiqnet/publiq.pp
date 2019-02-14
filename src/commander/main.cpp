#include "message.hpp"
#include "http.hpp"
#include "program_options.hpp"

#include <belt.pp/global.hpp>
#include <belt.pp/log.hpp>
#include <belt.pp/scope_helper.hpp>
#include <belt.pp/socket.hpp>

#include <mesh.pp/fileutility.hpp>
#include <mesh.pp/cryptoutility.hpp>
#include <mesh.pp/settings.hpp>

#include <publiq.pp/message.hpp>

#include <boost/locale.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>

#include <iostream>
#include <thread>
#include <memory>
#include <chrono>
#include <unordered_set>
#include <vector>
#include <exception>
#include <functional>
#include <csignal>

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
    try
    {
        //  boost filesystem UTF-8 support
        std::locale::global(boost::locale::generator().generate(""));
        boost::filesystem::path::imbue(std::locale());
    }
    catch (...)
    {}  //  don't care for exception, for now

    beltpp::ip_address server_address;
    beltpp::ip_address rpc_address;
    string prefix;

    if (false == process_command_line(argc, argv,
                                      prefix,
                                      server_address,
                                      rpc_address))
        return 1;
    //
    meshpp::config::set_public_key_prefix(prefix);
    //
    string application_name = "commander";
    if (prefix.empty() == false)
        application_name += "_" + prefix;
    application_name += "_" + server_address.to_string();

    meshpp::settings::set_application_name(application_name);
    meshpp::settings::set_data_directory(meshpp::config_directory_path().string());


    unique_ptr<beltpp::event_handler> m_ptr_eh(new beltpp::event_handler());
    unique_ptr<beltpp::socket> m_ptr_rpc_socket(new beltpp::socket(
                                   beltpp::getsocket<sf>(*m_ptr_eh)
                                   ));

    m_ptr_eh->set_timer(chrono::seconds(10));
    m_ptr_eh->add(*m_ptr_rpc_socket);

    m_ptr_rpc_socket->listen(rpc_address);

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
