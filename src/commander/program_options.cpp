#include "program_options.hpp"

#include <boost/program_options.hpp>

#include <sstream>
#include <iostream>

using std::cout;
using std::endl;
using std::string;

namespace program_options = boost::program_options;

bool process_command_line(int argc, char** argv,
                          std::string& prefix,
                          beltpp::ip_address& connect_to_address,
                          beltpp::ip_address& listen_on_address)
{
    string str_connect_to_address;
    string str_listen_on_address;

    program_options::options_description options_description;
    try
    {
        auto desc_init = options_description.add_options()
            ("help,h", "Print this help message and exit.")
            ("connect_to_address,c", program_options::value<string>(&str_connect_to_address)->required(),
                            "the blockchain daemon rpc address")
            ("listen_on_address,l", program_options::value<string>(&str_listen_on_address)->required(),
                            "commander rpc address")
            ("prefix,p", program_options::value<string>(&prefix)->required(),
                            "blockchain prefix");
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

        connect_to_address.from_string(str_connect_to_address);
        listen_on_address.from_string(str_listen_on_address);

        if (false == connect_to_address.remote.empty())
        {
            connect_to_address.local = connect_to_address.remote;
            connect_to_address.remote = beltpp::ip_destination();
        }
        if (listen_on_address.local.empty())
        {
            listen_on_address.local = listen_on_address.remote;
            listen_on_address.remote = beltpp::ip_destination();
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
