#include <belt.pp/global.hpp>
#include <belt.pp/log.hpp>
#include <belt.pp/scope_helper.hpp>

#include <mesh.pp/fileutility.hpp>
#include <mesh.pp/processutility.hpp>
#include <mesh.pp/cryptoutility.hpp>
#include <mesh.pp/log.hpp>
#include <mesh.pp/settings.hpp>
#include <mesh.pp/pid.hpp>

#include <publiq.pp/storage_utility_rpc.hpp>

#include <boost/program_options.hpp>
#include <boost/locale.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>

#include <memory>
#include <iostream>
#include <vector>
#include <sstream>
#include <exception>
#include <functional>
#include <chrono>
#include <map>

#include <csignal>

namespace program_options = boost::program_options;

using std::unique_ptr;
using std::string;
using std::cout;
using std::endl;
using std::vector;
using std::runtime_error;

bool process_command_line(int argc, char** argv,
                            std::string& prefix,
                            beltpp::ip_address& rpc_bind_to_address,
                            string& data_directory);

static bool g_termination_handled = false;
static storage_utility::rpc* g_prpc = nullptr;

void termination_handler(int /*signum*/)
{
    g_termination_handled = true;
    if (g_prpc)
        g_prpc->wake();
}

template <typename RPC>
void loop(RPC& rpc, beltpp::ilog_ptr& plogger_exceptions, bool& termination_handled);

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
    //
    meshpp::settings::set_application_name("storage_helper");
    meshpp::settings::set_data_directory(meshpp::config_directory_path().string());

    beltpp::ip_address rpc_bind_to_address;
    string data_directory;
    string prefix;

    if (false == process_command_line(argc, argv,
                                      prefix,
                                      rpc_bind_to_address,
                                      data_directory))
        return 1;

    meshpp::config::set_public_key_prefix(prefix);

    if (false == data_directory.empty())
        meshpp::settings::set_data_directory(data_directory);

#ifdef B_OS_WINDOWS
    signal(SIGINT, termination_handler);
#else
    struct sigaction signal_handler;
    signal_handler.sa_handler = termination_handler;
    ::sigaction(SIGINT, &signal_handler, nullptr);
    ::sigaction(SIGTERM, &signal_handler, nullptr);
#endif

    beltpp::ilog_ptr plogger_exceptions;

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

        auto fs_log = meshpp::data_directory_path("log");

        if (false == rpc_bind_to_address.local.empty())
            cout << "rpc interface: " << rpc_bind_to_address.to_string() << endl;

        beltpp::ilog_ptr plogger_rpc = beltpp::console_logger("storage_helper_rpc", true);
        //plogger_rpc->disable();
        plogger_exceptions = meshpp::file_logger("storage_helper_exceptions",
                                                 fs_log / "exceptions.txt");
        //__debugbreak();

        storage_utility::rpc rpc(rpc_bind_to_address,
                                 plogger_rpc.get());

        g_prpc = &rpc;

        loop(rpc, plogger_exceptions, g_termination_handled);

        dda->history.back().end.tm = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        dda.save();
    }
    catch (std::exception const& ex)
    {
        if (plogger_exceptions)
            plogger_exceptions->message(ex.what());
        cout << "exception cought: " << ex.what() << endl;
    }
    catch (...)
    {
        if (plogger_exceptions)
            plogger_exceptions->message("always throw std::exceptions");
        cout << "always throw std::exceptions" << endl;
    }
    return 0;
}

template <typename RPC>
void loop(RPC& rpc, beltpp::ilog_ptr& plogger_exceptions, bool& termination_handled)
{
    while (true)
    {
        try
        {
            if (termination_handled)
                break;
            if (false == rpc.run())
                break;
        }
        catch (std::bad_alloc const& ex)
        {
            if (plogger_exceptions)
                plogger_exceptions->message(ex.what());
            cout << "exception cought: " << ex.what() << endl;
            cout << "will exit now" << endl;
            termination_handler(0);
            break;
        }
        catch (std::logic_error const& ex)
        {
            if (plogger_exceptions)
                plogger_exceptions->message(ex.what());
            cout << "logic error cought: " << ex.what() << endl;
            cout << "will exit now" << endl;
            termination_handler(0);
            break;
        }
        catch (std::exception const& ex)
        {
            if (plogger_exceptions)
                plogger_exceptions->message(ex.what());
            cout << "exception cought: " << ex.what() << endl;
        }
        catch (...)
        {
            if (plogger_exceptions)
                plogger_exceptions->message("always throw std::exceptions, will exit now");
            cout << "always throw std::exceptions, will exit now" << endl;
            termination_handler(0);
            break;
        }
    }
}

bool process_command_line(int argc, char** argv,
                          std::string& prefix,
                          beltpp::ip_address& rpc_bind_to_address,
                          string& data_directory)
{
    string rpc_local_interface;
    vector<string> hosts;
    program_options::options_description options_description;
    try
    {
        auto desc_init = options_description.add_options()
            ("help,h", "Print this help message and exit.")
            ("rpc_local_interface,r", program_options::value<string>(&rpc_local_interface),
                            "(rpc) The local network interface and port to bind to")
            ("data_directory,d", program_options::value<string>(&data_directory),
                            "Data directory path")
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

        if (false == rpc_local_interface.empty())
            rpc_bind_to_address.from_string(rpc_local_interface);

        if (rpc_local_interface.empty())
            throw std::runtime_error("rpc_local_interface is not specified");
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
