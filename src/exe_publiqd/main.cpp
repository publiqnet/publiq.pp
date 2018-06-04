#include "message.hpp"
#include "settings.hpp"

#include "pid.hpp"

#include <belt.pp/socket.hpp>
#include <belt.pp/message_global.hpp>
#include <belt.pp/log.hpp>
#include <belt.pp/event.hpp>

#include <mesh.pp/fileutility.hpp>
#include <mesh.pp/processutility.hpp>

#include <publiq.pp/blockchainsocket.hpp>

#include <boost/program_options.hpp>

#include <boost/locale.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>

#include <memory>
#include <iostream>
#include <vector>
#include <sstream>
#include <chrono>
#include <memory>
#include <exception>
#include <unordered_set>

#include <signal.h>

using namespace PubliqNodeMessage;

using peer_id = publiqpp::blockchainsocket::peer_id;
using packet = publiqpp::blockchainsocket::packet;
using packets = publiqpp::blockchainsocket::packets;

namespace program_options = boost::program_options;

using std::unique_ptr;
using std::string;
using std::cout;
using std::endl;
using std::vector;
namespace chrono = std::chrono;
using chrono::steady_clock;
using std::unique_ptr;
using std::runtime_error;
using std::unordered_set;

using p2p_sf = publiqpp::blockchainsocket_family_t<
    Error::rtt,
    Join::rtt,
    Drop::rtt,
    &beltpp::new_void_unique_ptr<Error>,
    &beltpp::new_void_unique_ptr<Join>,
    &beltpp::new_void_unique_ptr<Drop>,
    &Error::saver,
    &Join::saver,
    &Drop::saver
>;

using sf = beltpp::socket_family_t<
    Error::rtt,
    Join::rtt,
    Drop::rtt,
    &beltpp::new_void_unique_ptr<Error>,
    &beltpp::new_void_unique_ptr<Join>,
    &beltpp::new_void_unique_ptr<Drop>,
    &Error::saver,
    &Join::saver,
    &Drop::saver,
    &message_list_load
>;

bool process_command_line(int argc, char** argv,
                          beltpp::ip_address& p2p_bind_to_address,
                          vector<beltpp::ip_address>& p2p_connect_to_addresses,
                          beltpp::ip_address& rpc_bind_to_address,
                          string& data_directory,
                          string& greeting,
                          bool& oneshot);

inline string short_name(string const& peerid)
{
    return peerid.substr(0, 5);
}

void add_port(Config::Port2PID& ob, unsigned short port)
{
    auto res = ob.reserved_ports.insert(std::make_pair(port, meshpp::current_process_id()));

    if (false == res.second)
    {
        string error = "port: ";
        error += std::to_string(res.first->first);
        error += " is locked by pid: ";
        error += std::to_string(res.first->second);
        throw runtime_error(error);
    }
}
void remove_port(Config::Port2PID& ob, unsigned short port)
{
    auto it = ob.reserved_ports.find(port);
    if (it == ob.reserved_ports.end())
        throw runtime_error("cannot find own port: " + std::to_string(port));

    ob.reserved_ports.erase(it);
}

bool g_termination_handled = false;
void termination_handler(int signum)
{
    g_termination_handled = true;
}


#include <cryptopp/sha.h>
#include <cryptopp/filters.h>
#include <cryptopp/base64.h>
#include <cryptopp/hex.h>

std::string SHA256HashString(std::string aString)
{
    byte digest[CryptoPP::SHA256::DIGESTSIZE];

    byte const* pb = (byte const*)aString.c_str();
    CryptoPP::SHA256().CalculateDigest(digest, pb, aString.length());

    CryptoPP::HexEncoder encoder;
    std::string output;

    encoder.Attach( new CryptoPP::StringSink( output ) );
    encoder.Put( digest, sizeof(digest) );
    encoder.MessageEnd();

    return output;
}

int main(int argc, char** argv)
{
    /*cout << SHA256HashString("a") << endl;
    return 0;*/
    //  boost filesystem UTF-8 support
    std::locale::global(boost::locale::generator().generate(""));
    boost::filesystem::path::imbue(std::locale());
    //
    settings::settings::set_application_name("publiqd");
    settings::settings::set_data_directory(settings::config_directory_path().string());

    beltpp::ip_address p2p_bind_to_address;
    beltpp::ip_address rpc_bind_to_address;
    vector<beltpp::ip_address> p2p_connect_to_addresses;
    string data_directory;
    string greeting;
    bool oneshot = false;

    if (false == process_command_line(argc, argv,
                                      p2p_bind_to_address,
                                      p2p_connect_to_addresses,
                                      rpc_bind_to_address,
                                      data_directory,
                                      greeting,
                                      oneshot))
        return 1;

    if (false == data_directory.empty())
        settings::settings::set_data_directory(data_directory);

    struct sigaction signal_handler;
    signal_handler.sa_handler = termination_handler;
    ::sigaction(SIGINT, &signal_handler, nullptr);
    ::sigaction(SIGINT, &signal_handler, nullptr);

    try
    {
        settings::create_config_directory();
        settings::create_data_directory();

        using port_toggler_type = void(*)(Config::Port2PID&, unsigned short);
        using FLPort2PID = meshpp::file_toggler<Config::Port2PID,
                                                port_toggler_type,
                                                port_toggler_type,
                                                &add_port,
                                                &remove_port,
                                                &Config::Port2PID::string_loader,
                                                &Config::Port2PID::string_saver,
                                                unsigned short>;

        FLPort2PID port2pid(settings::config_file_path("pid"), p2p_bind_to_address.local.port);

        using DataDirAttributeLoader = meshpp::file_locker<meshpp::file_loader<Config::DataDirAttribute, &Config::DataDirAttribute::string_loader, &Config::DataDirAttribute::string_saver>>;
        DataDirAttributeLoader dda(settings::data_file_path("running.txt"));
        Config::RunningDuration item;
        item.start.tm = item.end.tm = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

        dda->history.push_back(item);
        dda.next_layer().commit();

        auto fs_blockchain = settings::data_directory_path("blockchain");

        cout << p2p_bind_to_address.to_string() << endl;
        for (auto const& item : p2p_connect_to_addresses)
            cout << item.to_string() << endl;

        beltpp::message_loader_utility utl;
        PubliqNodeMessage::detail::extension_helper(utl);
        auto ptr_utl =
                beltpp::new_void_unique_ptr<beltpp::message_loader_utility>(std::move(utl));

        beltpp::ilog_ptr plogger = beltpp::console_logger("exe_publiqd");
        plogger->disable();

        beltpp::event_handler eh;
        eh.set_timer(chrono::seconds(10));

        publiqpp::blockchainsocket p2p_sk = publiqpp::getblockchainsocket<p2p_sf>(eh,
                                                                                  p2p_bind_to_address,
                                                                                  p2p_connect_to_addresses,
                                                                                  std::move(ptr_utl),
                                                                                  fs_blockchain,
                                                                                  plogger.get());

        beltpp::socket sk = beltpp::getsocket<sf>(eh);

        cout << endl;
        cout << "Node: " << short_name(p2p_sk.name()) << endl;
        cout << endl;

        sk.listen(rpc_bind_to_address);

        eh.add(sk);
        eh.add(p2p_sk);

        unordered_set<string> p2p_peers;

        while (true)
        {
            unordered_set<beltpp::ievent_item const*> wait_sockets;

            bool p2p_timer_event = false;

            //cout << "eh.wait" << endl;
            auto wait_result = eh.wait(wait_sockets);
            //cout << "done" << endl;

            if (wait_result == beltpp::event_handler::event &&
                wait_sockets.end() != wait_sockets.find(&sk))
            {
            try
            {
                beltpp::socket::peer_id peerid;

                //cout << "sk.receive" << endl;
                packets received_packets = sk.receive(peerid);
                //cout << "done" << endl;

                for (auto const& received_packet : received_packets)
                {
                    switch (received_packet.type())
                    {
                    case Join::rtt:
                    {
                        cout << peerid << " joined" << endl;
                        break;
                    }
                    case Drop::rtt:
                    {
                        cout << peerid << " dropped" << endl;
                        break;
                    }
                    case Error::rtt:
                    {
                        cout << peerid << " error" << endl;
                        sk.send(peerid, Drop());
                        break;
                    }
                    case Hellow::rtt:
                    {
                        Hellow hellow_msg;
                        received_packet.get(hellow_msg);

                        cout << "Hellow: " << hellow_msg.text << endl;
                        cout << "From: " << peerid << endl;
                        break;
                    }
                    case Broadcast::rtt:
                    {
                        Broadcast broadcast_msg;
                        received_packet.get(broadcast_msg);

                        cout << "broadcasting" << endl;

                        for (auto const& p2p_peer : p2p_peers)
                            p2p_sk.send(p2p_peer, std::move(broadcast_msg.payload));

                        break;
                    }
                    }
                }
            }
            catch (std::exception const& ex)
            {
                cout << "exception cought: " << ex.what() << endl;
            }
            catch (...)
            {
                cout << "always throw std::exceptions, will exit now" << endl;
                break;
            }
            }
            else if (wait_result == beltpp::event_handler::event &&
                     wait_sockets.end() != wait_sockets.find(&p2p_sk.worker()))
            {
            try
            {
                publiqpp::blockchainsocket::peer_id peerid;

                //cout << "p2p_sk.receive" << endl;
                packets received_packets = p2p_sk.receive(peerid);
                //cout << "done" << endl;

                for (auto const& received_packet : received_packets)
                {
                    switch (received_packet.type())
                    {
                    case Join::rtt:
                    {
                        cout << short_name(peerid) << " joined" << endl;

                        p2p_peers.insert(peerid);

                        if (greeting.empty() == false)
                        {
                            cout << "sending Hellow to: "
                                 << short_name(peerid)
                                 << endl;

                            Hellow msg_send;
                            msg_send.text = greeting;
                            p2p_sk.send(peerid, msg_send);
                        }
                        break;
                    }
                    case Drop::rtt:
                    {
                        cout << short_name(peerid) << " dropped" << endl;
                        p2p_peers.erase(peerid);
                        break;
                    }
                    case Hellow::rtt:
                    {
                        Hellow hellow_msg;
                        received_packet.get(hellow_msg);

                        cout << "Hellow: " << hellow_msg.text << endl;
                        cout << "From: " << short_name(peerid) << endl;
                        break;
                    }
                    }
                }
            }
            catch (std::exception const& ex)
            {
                cout << "exception cought: " << ex.what() << endl;
            }
            catch (...)
            {
                cout << "always throw std::exceptions, will exit now" << endl;
                break;
            }
            }
            else if (beltpp::event_handler::timer_out == wait_result)
            {
                cout << "timer" << endl;
                p2p_timer_event = true;
                sk.timer_action();
                p2p_sk.timer_action();
            }

            if (g_termination_handled ||
                (p2p_timer_event && oneshot))
                break;
        }

        dda->history.back().end.tm = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        dda.next_layer().commit();
        port2pid.commit();
    }
    catch (std::exception const& ex)
    {
        cout << "exception cought: " << ex.what() << endl;
    }
    catch (...)
    {
        cout << "always throw std::exceptions" << endl;
    }
    return 0;
}

bool process_command_line(int argc, char** argv,
                          beltpp::ip_address& p2p_bind_to_address,
                          vector<beltpp::ip_address>& p2p_connect_to_addresses,
                          beltpp::ip_address& rpc_bind_to_address,
                          string& data_directory,
                          string& greeting,
                          bool& oneshot)
{
    string p2p_local_interface;
    string rpc_local_interface;
    vector<string> hosts;
    program_options::options_description options_description;
    try
    {
        auto desc_init = options_description.add_options()
            ("help,h", "Print this help message and exit.")
            ("p2p_local_interface,i", program_options::value<string>(&p2p_local_interface)->required(),
                            "The local network interface and port to bind to")
            ("p2p_remote_host,p", program_options::value<vector<string>>(&hosts),
                            "Remote nodes addresss with port")
            ("rpc_local_interface,r", program_options::value<string>(&rpc_local_interface)->required(),
                            "The local network interface and port to bind to")
            ("data_directory,d", program_options::value<string>(&data_directory),
                            "Data directory path")
            ("greeting,g", program_options::value<string>(&greeting),
                            "send a greeting message to all peers")
            ("oneshot,1", "set to exit after timer event");
        (void)(desc_init);

        program_options::variables_map options;

        program_options::store(
                    program_options::parse_command_line(argc, argv, options_description),
                    options);

        program_options::notify(options);

        if (options.count("help"))
        {
            throw std::runtime_error("");
        }
        if (options.count("oneshot"))
            oneshot = true;

        p2p_bind_to_address.from_string(p2p_local_interface);
        rpc_bind_to_address.from_string(rpc_local_interface);
        for (auto const& item : hosts)
        {
            beltpp::ip_address address_item;
            address_item.from_string(item);
            p2p_connect_to_addresses.push_back(address_item);
        }
    }
    catch (std::exception const& ex)
    {
        std::stringstream ss;
        ss << options_description;

        string ex_message = ex.what();
        if (false == ex_message.empty())
            cout << ex.what() << endl << endl;
        cout << ss.str();
        return false;
    }
    catch (...)
    {
        cout << "always throw std::exceptions" << endl;
        return false;
    }

    return true;
}
