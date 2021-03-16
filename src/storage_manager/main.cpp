#include "program_options.hpp"
#include "manager.hpp"

#include <belt.pp/global.hpp>
#include <belt.pp/scope_helper.hpp>

#include <mesh.pp/fileutility.hpp>
#include <mesh.pp/cryptoutility.hpp>
#include <mesh.pp/settings.hpp>
#include <mesh.pp/pid.hpp>

#include <publiq.pp/message.hpp>
#include <publiq.pp/message.tmpl.hpp>

#include <boost/locale.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>

#include <iostream>
#include <thread>
#include <memory>
#include <vector>
#include <exception>
#include <stdexcept>
#include <functional>
#include <csignal>
#include <string>

using std::cout;
using std::endl;
using std::unique_ptr;
using std::string;

static bool g_termination_handled = false;
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

    string prefix;
    string str_pv_key;
    uint64_t sync_interval;
    beltpp::ip_address connect_to_address;
    beltpp::ip_address manager_address;
    string data_directory;

    if (false == process_command_line(argc, argv,
                                      prefix,
                                      str_pv_key,
                                      sync_interval,
                                      connect_to_address,
                                      manager_address,
                                      data_directory))
        return 1;

#ifdef B_OS_WINDOWS
    signal(SIGINT, termination_handler);
#else
    struct sigaction signal_handler;
    signal_handler.sa_handler = termination_handler;
    ::sigaction(SIGINT, &signal_handler, nullptr);
    ::sigaction(SIGTERM, &signal_handler, nullptr);
#endif

    //
    meshpp::config::set_public_key_prefix(prefix);
    //
    string application_name = "storage_manager";
    if (prefix.empty() == false)
        application_name += "_" + prefix;
    string add_to_dir = connect_to_address.to_string();
    replace(add_to_dir.begin(), add_to_dir.end(), ':', '_');
    application_name += "_" + add_to_dir;

    meshpp::settings::set_application_name(application_name);
    meshpp::settings::set_data_directory(data_directory);

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

        manager server(str_pv_key, manager_address, connect_to_address, sync_interval);

        while (true)
        {
            try
            {
                if (g_termination_handled)
                    break;
                server.run();
            }
            catch (std::logic_error const& ex)
            {
                cout << "logic error cought: " << ex.what() << endl;
                cout << "will exit now" << endl;
                termination_handler(0);
                break;
            }
            catch(std::exception const& ex)
            {
                cout << "exception cought: " << ex.what() << endl;
            }
            catch(...)
            {
                cout << "always throw std::exceptions" << endl;
                termination_handler(0);
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
