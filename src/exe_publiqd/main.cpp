#include "message.hpp"

#include <belt.pp/message_global.hpp>

#include <mesh.pp/p2psocket.hpp>

#include <boost/program_options.hpp>

#include <memory>
#include <iostream>
#include <vector>
#include <sstream>

using namespace PubliqNodeMessage;

namespace program_options = boost::program_options;

using std::unique_ptr;
using std::string;
using std::cout;
using std::endl;
using std::vector;

using sf = meshpp::p2psocket_family_t<
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
                          vector<beltpp::ip_address>& connect_to_addresses)
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
                            "Remote nodes addresss with port");
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

int main(int argc, char** argv)
{
    beltpp::ip_address bind_to_address;
    vector<beltpp::ip_address> connect_to_addresses;

    if (false == process_command_line(argc, argv,
                                      bind_to_address,
                                      connect_to_addresses))
        return 1;

    try
    {
        cout << bind_to_address.to_string() << endl;
        for (auto const& item : connect_to_addresses)
            cout << item.to_string() << endl;


        beltpp::message_loader_utility utl;
        PubliqNodeMessage::detail::extension_helper(utl);
        auto ptr_utl =
                beltpp::new_void_unique_ptr<beltpp::message_loader_utility>(&utl);

        meshpp::p2psocket sk = meshpp::getp2psocket<sf>(bind_to_address,
                                                        connect_to_addresses,
                                                        std::move(ptr_utl),
                                                        nullptr);
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
