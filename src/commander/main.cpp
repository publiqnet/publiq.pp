#include "program_options.hpp"
#include "rpc.hpp"

#include <belt.pp/global.hpp>
#include <belt.pp/scope_helper.hpp>

#include <mesh.pp/fileutility.hpp>
#include <mesh.pp/cryptoutility.hpp>
#include <mesh.pp/settings.hpp>
#include <mesh.pp/pid.hpp>

#include <publiq.pp/message.hpp>

#include <boost/locale.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>

#include <iostream>
#include <thread>
#include <memory>
#include <vector>
#include <exception>
#include <functional>
#include <csignal>
#include <string>

using std::cout;
using std::endl;
using std::unique_ptr;
using std::string;

bool g_termination_handled = false;
void termination_handler(int /*signum*/)
{
    g_termination_handled = true;
}

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

    beltpp::ip_address connect_to_address;
    beltpp::ip_address rpc_address;
    string prefix;

    if (false == process_command_line(argc, argv,
                                      prefix,
                                      connect_to_address,
                                      rpc_address))
        return 1;
    //
    meshpp::config::set_public_key_prefix(prefix);
    //
    string application_name = "commander";
    if (prefix.empty() == false)
        application_name += "_" + prefix;
    application_name += "_" + connect_to_address.to_string();

    meshpp::settings::set_application_name(application_name);
    meshpp::settings::set_data_directory(meshpp::config_directory_path().string());

    try
    {
        meshpp::create_config_directory();
        meshpp::create_data_directory();

        using DataDirAttributeLoader = meshpp::file_locker<meshpp::file_loader<PidConfig::DataDirAttribute,
                                                                                &PidConfig::DataDirAttribute::from_string,
                                                                                &PidConfig::DataDirAttribute::to_string>>;
        DataDirAttributeLoader dda(meshpp::data_file_path("running.txt"));
        {
            PidConfig::RunningDuration item;
            item.start.tm = item.end.tm = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

            dda->history.push_back(item);
            dda.save();
        }

        rpc server(rpc_address, connect_to_address);

        while (true)
        {
        try
        {
            if (g_termination_handled)
                break;
            server.run();
        }
        catch(std::exception const& ex)
        {
            cout << "exception cought: " << ex.what() << endl;
        }
        catch(...)
        {
            cout << "always throw std::exceptions" << endl;
            break;
        }
        }

        dda->history.back().end.tm = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        dda.save();
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
