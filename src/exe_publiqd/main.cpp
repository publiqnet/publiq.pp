#include "settings.hpp"

#include "pid.hpp"

#include <belt.pp/log.hpp>

#include <mesh.pp/fileutility.hpp>
#include <mesh.pp/processutility.hpp>

#include <publiq.pp/node.hpp>

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

namespace program_options = boost::program_options;

using std::unique_ptr;
using std::string;
using std::cout;
using std::endl;
using std::vector;
namespace chrono = std::chrono;
using chrono::steady_clock;
using std::runtime_error;

bool process_command_line(int argc, char** argv,
                          beltpp::ip_address& p2p_bind_to_address,
                          vector<beltpp::ip_address>& p2p_connect_to_addresses,
                          beltpp::ip_address& rpc_bind_to_address,
                          string& data_directory);

bool g_termination_handled = false;
void termination_handler(int signum)
{
    g_termination_handled = true;
}

class port2pid_helper
{
    using Loader = meshpp::file_locker<meshpp::file_loader<Config::Port2PID,
                                                            &Config::Port2PID::string_loader,
                                                            &Config::Port2PID::string_saver>>;
public:
    port2pid_helper(boost::filesystem::path const& _path, unsigned short _port)
        : port(_port)
        , path(_path)
        , eptr()
    {
        Loader ob(path);
        auto res = ob->reserved_ports.insert(std::make_pair(port, meshpp::current_process_id()));

        if (false == res.second)
        {
            string error = "port: ";
            error += std::to_string(res.first->first);
            error += " is locked by pid: ";
            error += std::to_string(res.first->second);
            error += " as specified in: ";
            error += path.string();
            throw runtime_error(error);
        }

        ob.save();
    }
    ~port2pid_helper()
    {
        _commit();
    }

    void commit()
    {
        _commit();

        if (eptr)
            std::rethrow_exception(eptr);
    }
private:
    void _commit()
    {
        try
        {
            Loader ob(path);
            auto it = ob.as_const()->reserved_ports.find(port);
            if (it == ob.as_const()->reserved_ports.end())
            {
                string error = "cannot find own port: ";
                error += std::to_string(port);
                error += " specified in: ";
                error += path.string();
                throw runtime_error(error);
            }

            ob->reserved_ports.erase(it);
            ob.save();
        }
        catch (...)
        {
            eptr = std::current_exception();
        }
    }
    unsigned short port;
    boost::filesystem::path path;
    std::exception_ptr eptr;
};

int main(int argc, char** argv)
{
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

    if (false == process_command_line(argc, argv,
                                      p2p_bind_to_address,
                                      p2p_connect_to_addresses,
                                      rpc_bind_to_address,
                                      data_directory))
        return 1;

    if (false == data_directory.empty())
        settings::settings::set_data_directory(data_directory);

#ifdef B_OS_WINDOWS
    // will work after wait retuns, not immediately !
    signal(SIGINT, termination_handler);
#else
    struct sigaction signal_handler;
    signal_handler.sa_handler = termination_handler;
    ::sigaction(SIGINT, &signal_handler, nullptr);
    ::sigaction(SIGTERM, &signal_handler, nullptr);
#endif

    try
    {
        settings::create_config_directory();
        settings::create_data_directory();

        unique_ptr<port2pid_helper> port2pid(new port2pid_helper(settings::config_file_path("pid"), p2p_bind_to_address.local.port));

        using DataDirAttributeLoader = meshpp::file_locker<meshpp::file_loader<Config::DataDirAttribute, &Config::DataDirAttribute::string_loader, &Config::DataDirAttribute::string_saver>>;
        DataDirAttributeLoader dda(settings::data_file_path("running.txt"));
        Config::RunningDuration item;
        item.start.tm = item.end.tm = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

        dda->history.push_back(item);
        dda.save();

        auto fs_blockchain = settings::data_directory_path("blockchain");
        auto fs_action_log = settings::data_directory_path("action_log");
        auto fs_storage = settings::data_directory_path("storage");

        cout << p2p_bind_to_address.to_string() << endl;
        for (auto const& item : p2p_connect_to_addresses)
            cout << item.to_string() << endl;

        beltpp::ilog_ptr plogger_p2p = beltpp::console_logger("exe_publiqd_p2p");
        plogger_p2p->disable();
        beltpp::ilog_ptr plogger_rpc = beltpp::console_logger("exe_publiqd_rpc");
        //plogger_rpc->disable();

        publiqpp::node node(rpc_bind_to_address,
                            p2p_bind_to_address,
                            p2p_connect_to_addresses,
                            fs_blockchain,
                            fs_action_log,
                            fs_storage,
                            plogger_p2p.get(),
                            plogger_rpc.get());

        cout << endl;
        cout << "Node: " << node.name()/*.substr(0, 5)*/ << endl;
        cout << endl;

        while (true)
        {
            try
            {
            if (false == node.run())
                break;

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

        dda->history.back().end.tm = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        dda.save();
        port2pid->commit();
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
                          string& data_directory)
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
                            "Data directory path");
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
