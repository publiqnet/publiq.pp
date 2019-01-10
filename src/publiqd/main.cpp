#include "settings.hpp"

#include "pid.hpp"

#include <belt.pp/global.hpp>
#include <belt.pp/log.hpp>

#include <mesh.pp/fileutility.hpp>
#include <mesh.pp/processutility.hpp>
#include <mesh.pp/cryptoutility.hpp>
#include <mesh.pp/log.hpp>

#include <publiq.pp/node.hpp>
#include <publiq.pp/storage_node.hpp>

#include <boost/program_options.hpp>
#include <boost/locale.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>

#include <memory>
#include <iostream>
#include <vector>
#include <sstream>
#include <exception>
#include <thread>
#include <functional>

#include <csignal>

using namespace BlockchainMessage;
namespace program_options = boost::program_options;

using std::unique_ptr;
using std::string;
using std::cout;
using std::endl;
using std::vector;
using std::runtime_error;

bool process_command_line(int argc, char** argv,
                          beltpp::ip_address& p2p_bind_to_address,
                          vector<beltpp::ip_address>& p2p_connect_to_addresses,
                          beltpp::ip_address& rpc_bind_to_address,
                          string& data_directory,
                          string& admin_ip_address,
                          meshpp::private_key& pv_key,
                          NodeType& n_type,
                          bool& log_enabled);

static bool g_termination_handled = false;
static publiqpp::node* g_pnode = nullptr;
static publiqpp::storage_node* g_pstorage_node = nullptr;
void termination_handler(int /*signum*/)
{
    g_termination_handled = true;
    if (g_pnode)
        g_pnode->terminate();
    if (g_pstorage_node)
        g_pstorage_node->terminate();
}

class port2pid_helper
{
    using Loader = meshpp::file_locker<meshpp::file_loader<Config::Port2PID,
                                                            &Config::Port2PID::from_string,
                                                            &Config::Port2PID::to_string>>;
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
    port2pid_helper(port2pid_helper const&) = delete;
    port2pid_helper(port2pid_helper&&) = delete;
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

template <typename NODE>
void loop(NODE& node, beltpp::ilog_ptr& plogger_exceptions, bool& termination_handled);

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
    meshpp::config::set_public_key_prefix("PBQ");
    //
    settings::settings::set_application_name("publiqd");
    settings::settings::set_data_directory(settings::config_directory_path().string());

    beltpp::ip_address p2p_bind_to_address;
    beltpp::ip_address rpc_bind_to_address;
    vector<beltpp::ip_address> p2p_connect_to_addresses;
    string data_directory;
    NodeType n_type = NodeType::miner;
    bool log_enabled = false;
    meshpp::random_seed seed;
    meshpp::private_key pv_key = seed.get_private_key(0);
    string admin_ip_address;

    if (false == process_command_line(argc, argv,
                                      p2p_bind_to_address,
                                      p2p_connect_to_addresses,
                                      rpc_bind_to_address,
                                      data_directory,
                                      admin_ip_address,
                                      pv_key,
                                      n_type,
                                      log_enabled))
        return 1;

    if (false == data_directory.empty())
        settings::settings::set_data_directory(data_directory);

#ifdef B_OS_WINDOWS
    signal(SIGINT, termination_handler);
#else
    struct sigaction signal_handler;
    signal_handler.sa_handler = termination_handler;
    ::sigaction(SIGINT, &signal_handler, nullptr);
    ::sigaction(SIGTERM, &signal_handler, nullptr);
#endif

    beltpp::ilog_ptr plogger_exceptions = beltpp::t_unique_nullptr<beltpp::ilog>();
    beltpp::ilog_ptr plogger_storage_exceptions = beltpp::t_unique_nullptr<beltpp::ilog>();

    try
    {
        settings::create_config_directory();
        settings::create_data_directory();

        unique_ptr<port2pid_helper> port2pid(new port2pid_helper(settings::config_file_path("pid"), p2p_bind_to_address.local.port));

        using DataDirAttributeLoader = meshpp::file_locker<meshpp::file_loader<Config::DataDirAttribute,
                                                                                &Config::DataDirAttribute::from_string,
                                                                                &Config::DataDirAttribute::to_string>>;
        DataDirAttributeLoader dda(settings::data_file_path("running.txt"));
        {
            Config::RunningDuration item;
            item.start.tm = item.end.tm = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

            dda->history.push_back(item);
            dda.save();
        }

        auto fs_blockchain = settings::data_directory_path("blockchain");
        auto fs_action_log = settings::data_directory_path("action_log");
        auto fs_storage = settings::data_directory_path("storage");
        auto fs_transaction_pool = settings::data_directory_path("transaction_pool");
        auto fs_state = settings::data_directory_path("state");
        auto fs_log = settings::data_directory_path("log");

        cout << "p2p local address: " << p2p_bind_to_address.to_string() << endl;
        for (auto const& item : p2p_connect_to_addresses)
            cout << "p2p host: " << item.to_string() << endl;
        if (false == rpc_bind_to_address.local.empty())
            cout << "rpc interface: " << rpc_bind_to_address.to_string() << endl;

        beltpp::ilog_ptr plogger_p2p = beltpp::console_logger("exe_publiqd_p2p", false);
        plogger_p2p->disable();
        beltpp::ilog_ptr plogger_rpc = beltpp::console_logger("exe_publiqd_rpc", true);
        //plogger_rpc->disable();
        plogger_exceptions = meshpp::file_logger("publiqd_exceptions",
                                                 fs_log / "exceptions.txt");
        plogger_storage_exceptions = meshpp::file_logger("storage_exceptions",
                                                         fs_log / "storage_exceptions.txt");

        //__debugbreak();

        publiqpp::node node(rpc_bind_to_address,
                            p2p_bind_to_address,
                            p2p_connect_to_addresses,
                            fs_blockchain,
                            fs_action_log,
                            fs_storage,
                            fs_transaction_pool,
                            fs_state,
                            plogger_p2p.get(),
                            plogger_rpc.get(),
                            pv_key,
                            n_type,
                            log_enabled);

        auto storage_bind_to_address = rpc_bind_to_address;
        storage_bind_to_address.local.port++;

        publiqpp::storage_node storage_node(storage_bind_to_address,
                                            fs_storage,
                                            plogger_rpc.get());

        cout << endl;
        cout << "Node: " << node.name() << endl;
        cout << "Type: " << static_cast<int>(n_type) << endl;
        cout << endl;

        g_pnode = &node;
        g_pstorage_node = &storage_node;

        std::thread node_thread([&node, &plogger_exceptions]
        {
            loop(node, plogger_exceptions, g_termination_handled);
        });
        std::thread storage_node_thread([&storage_node, &plogger_storage_exceptions]
        {
            loop(storage_node, plogger_storage_exceptions, g_termination_handled);
        });

        node_thread.join();
        storage_node_thread.join();

        dda->history.back().end.tm = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        dda.save();
        port2pid->commit();
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

template <typename NODE>
void loop(NODE& node, beltpp::ilog_ptr& plogger_exceptions, bool& termination_handled)
{
    while (true)
    {
        try
        {
            if (termination_handled)
                break;
            if (false == node.run())
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
                          beltpp::ip_address& p2p_bind_to_address,
                          vector<beltpp::ip_address>& p2p_connect_to_addresses,
                          beltpp::ip_address& rpc_bind_to_address,
                          string& data_directory,
                          string& admin_ip_address,
                          meshpp::private_key& pv_key,
                          NodeType& n_type,
                          bool& log_enabled)
{
    string p2p_local_interface;
    string rpc_local_interface;
    string str_pv_key;
    string str_n_type;
    vector<string> hosts;
    program_options::options_description options_description;
    try
    {
        auto desc_init = options_description.add_options()
            ("help,h", "Print this help message and exit.")
            ("p2p_local_interface,i", program_options::value<string>(&p2p_local_interface)->required(),
                            "(p2p) The local network interface and port to bind to")
            ("p2p_remote_host,p", program_options::value<vector<string>>(&hosts),
                            "Remote nodes addresss with port")
            ("rpc_local_interface,r", program_options::value<string>(&rpc_local_interface),
                            "(rpc) The local network interface and port to bind to")
            ("data_directory,d", program_options::value<string>(&data_directory),
                            "Data directory path")
            ("node_private_key,k", program_options::value<string>(&str_pv_key),
                            "Node private key to start with")
            ("node_type,t", program_options::value<string>(&str_n_type)->required(),
                            "Node start mode")
            ("admin_ip_address,a", program_options::value<string>(&admin_ip_address),
                            "The IP address allowed to make admin calls over rpc. Not set means everyone's allowed");
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
        if (false == rpc_local_interface.empty())
            rpc_bind_to_address.from_string(rpc_local_interface);

        for (auto const& item : hosts)
        {
            beltpp::ip_address address_item;
            address_item.from_string(item);
            p2p_connect_to_addresses.push_back(address_item);
        }

        if (p2p_connect_to_addresses.empty())
        {
            beltpp::ip_address address_item;
            address_item.from_string("north.publiq.network:12222");
            p2p_connect_to_addresses.push_back(address_item);
        }

        if (false == str_pv_key.empty())
            pv_key = meshpp::private_key(str_pv_key);
        else
            log_enabled = true;

        BlockchainMessage::detail::from_string(str_n_type, n_type);
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
