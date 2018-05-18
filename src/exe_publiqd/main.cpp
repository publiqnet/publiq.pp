#include "message.hpp"
#include "settings.hpp"

#include "pid.hpp"

#include <belt.pp/message_global.hpp>
#include <belt.pp/log.hpp>

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

using sf = publiqpp::blockchainsocket_family_t<
    Error::rtt,
    Join::rtt,
    Drop::rtt,
    TimerOut::rtt,
    &beltpp::new_void_unique_ptr<Error>,
    &beltpp::new_void_unique_ptr<Join>,
    &beltpp::new_void_unique_ptr<Drop>,
    &beltpp::new_void_unique_ptr<TimerOut>,
    &Error::saver,
    &Join::saver,
    &Drop::saver,
    &TimerOut::saver
>;

bool process_command_line(int argc, char** argv,
                          beltpp::ip_address& bind_to_address,
                          vector<beltpp::ip_address>& connect_to_addresses,
                          string& greeting);

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

int main(int argc, char** argv)
{
    //  boost filesystem UTF-8 support
    std::locale::global(boost::locale::generator().generate(""));
    boost::filesystem::path::imbue(std::locale());
    //
    settings::settings::set_application_name("publiqd");
    settings::settings::set_data_dir(settings::config_dir_path().string());

    beltpp::ip_address bind_to_address;
    vector<beltpp::ip_address> connect_to_addresses;
    string greeting;

    if (false == process_command_line(argc, argv,
                                      bind_to_address,
                                      connect_to_addresses,
                                      greeting))
        return 1;

    struct sigaction signal_handler;
    signal_handler.sa_handler = termination_handler;
    sigaction(SIGINT, &signal_handler, NULL);
    sigaction(SIGINT, &signal_handler, NULL);

    try
    {
        settings::create_config_dir();
        settings::create_data_dir();

        using port_toggler_type = void(*)(Config::Port2PID&, unsigned short);
        using FLPort2PID = meshpp::file_toggler<Config::Port2PID,
                                                port_toggler_type,
                                                port_toggler_type,
                                                &add_port,
                                                &remove_port,
                                                &Config::Port2PID::string_loader,
                                                &Config::Port2PID::string_saver,
                                                unsigned short>;

        FLPort2PID port2pid(settings::config_file_path("pid"), bind_to_address.local.port);

        cout << bind_to_address.to_string() << endl;
        for (auto const& item : connect_to_addresses)
            cout << item.to_string() << endl;


        beltpp::message_loader_utility utl;
        PubliqNodeMessage::detail::extension_helper(utl);
        auto ptr_utl =
                beltpp::new_void_unique_ptr<beltpp::message_loader_utility>(&utl);

        beltpp::ilog_ptr plogger = beltpp::console_logger("exe_publiqd");
        plogger->disable();

        publiqpp::blockchainsocket sk = publiqpp::getblockchainsocket<sf>(bind_to_address,
                                                                          connect_to_addresses,
                                                                          std::move(ptr_utl),
                                                                          plogger.get());
        sk.set_timer(chrono::seconds(10));

        cout << endl;
        cout << "Node: " << short_name(sk.name()) << endl;
        cout << endl;

        while (true)
        {
            try
            {
                publiqpp::blockchainsocket::peer_id peerid;

                packets received_packets = sk.receive(peerid);

                for (auto const& received_packet : received_packets)
                {
                    switch (received_packet.type())
                    {
                    case Join::rtt:
                    {
                        cout << short_name(peerid) << " joined" << endl;

                        if (greeting.empty() == false)
                        {
                            cout << "sending Hellow to: "
                                 << short_name(peerid)
                                 << endl;

                            Hellow msg_send;
                            msg_send.text = greeting;
                            sk.send(peerid, msg_send);
                        }
                        break;
                    }
                    case Drop::rtt:
                        cout << short_name(peerid) << " dropped" << endl;
                        break;
                    case TimerOut::rtt:
                        cout << "timer" << endl;
                        break;
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

                if (g_termination_handled)
                    break;
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
                          beltpp::ip_address& bind_to_address,
                          vector<beltpp::ip_address>& connect_to_addresses,
                          string& greeting)
{
    string interface;
    vector<string> hosts;
    program_options::options_description options_description;
    try
    {
        auto desc_init = options_description.add_options()
            ("help,h", "Print this help message and exit.")
            ("interface,i", program_options::value<string>(&interface)->required(),
                            "The local network interface and port to bind to")
            ("connect,c", program_options::value<vector<string>>(&hosts),
                            "Remote nodes addresss with port")
            ("greeting,g", program_options::value<string>(&greeting),
                            "send a greeting message to all peers");
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

        bind_to_address.from_string(interface);
        for (auto const& item : hosts)
        {
            beltpp::ip_address address_item;
            address_item.from_string(item);
            connect_to_addresses.push_back(address_item);
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
