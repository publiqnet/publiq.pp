#include <publiq.pp/network.hpp>

#include <belt.pp/global.hpp>
#include <belt.pp/log.hpp>
#include <belt.pp/scope_helper.hpp>

#include <mesh.pp/fileutility.hpp>
#include <mesh.pp/processutility.hpp>
#include <mesh.pp/cryptoutility.hpp>
#include <mesh.pp/log.hpp>
#include <mesh.pp/settings.hpp>
#include <mesh.pp/pid.hpp>

#include <publiq.pp/node.hpp>
#include <publiq.pp/storage_node.hpp>
#include <publiq.pp/coin.hpp>
#include <publiq.pp/config.hpp>

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
#include <unordered_set>

#include <thread>
#include <csignal>

using namespace network_simulation_impl;

using namespace BlockchainMessage;
namespace program_options = boost::program_options;

using std::unique_ptr;
using std::string;
using std::unordered_set;
using std::cout;
using std::endl;
using std::vector;
using std::runtime_error;
namespace chrono = std::chrono;
using chrono::system_clock;
using time_point = chrono::system_clock::time_point;

string genesis_signed_block(bool testnet);
publiqpp::coin mine_amount_threshhold();
vector<publiqpp::coin> block_reward_array();

using DataDirAttributeLoader = meshpp::file_locker<meshpp::file_loader<PidConfig::DataDirAttribute,
                                                                       &PidConfig::DataDirAttribute::from_string,
                                                                       &PidConfig::DataDirAttribute::to_string>>;

static bool g_termination_handled = false;

void termination_handler(int /*signum*/)
{
    cout << "stopping..." << endl;

    g_termination_handled = true;
}


uint64_t counts_per_channel_views(std::map<uint64_t, std::map<string, std::map<string, uint64_t>>> const& item_per_owner,
                                  uint64_t block_number,
                                  bool is_testnet)
{
    uint64_t count = 0;
    for (auto const& item_per_content_id : item_per_owner)
    {
        uint64_t max_count_per_content_id = 0;
        for (auto const& item_per_unit : item_per_content_id.second)
            for (auto const& item_per_file : item_per_unit.second)
            {
                if (false == is_testnet &&
                    (
                        block_number == 30335 ||
                        block_number == 30438 ||
                        block_number == 30346 ||
                        block_number == 30460 ||
                        block_number == 30463 ||
                        block_number == 30478
                        ))
                    max_count_per_content_id = std::max(count, item_per_file.second);
                else
                    max_count_per_content_id = std::max(max_count_per_content_id, item_per_file.second);
            }

        count += max_count_per_content_id;
    }

    return count;
}

bool content_unit_validate_check(std::vector<std::string> const& content_unit_file_uris,
                                 std::string& find_duplicate,
                                 uint64_t block_number,
                                 bool is_testnet)
{
    bool skip = false;
    if (is_testnet &&
        (
            block_number == 42846 ||
            block_number == 44727 ||
            block_number == 45433
            )
        )
        skip = true;

    if (skip)
        return true;

    unordered_set<string> file_uris;
    for (auto const& file_uri : content_unit_file_uris)
    {
        auto insert_res = file_uris.insert(file_uri);
        if (false == insert_res.second)
        {
            find_duplicate = file_uri;
            return false;
        }
    }

    return true;
}

struct node_info
{
    string data_dir;

    beltpp::ilog_ptr plogger_p2p;
    beltpp::ilog_ptr plogger_rpc;
    beltpp::ilog_ptr plogger_exceptions;

    event_handler_ns* peh;
    unique_ptr<publiqpp::node> node;
    unique_ptr<DataDirAttributeLoader> dda;
    publiqpp::config config;
};

bool process_command_line(int argc, char** argv,
                          string& data_directory_root,
                          size_t& node_count,
                          uint32_t& refuze_base);

void run_nodes(vector<node_info>& nodes)
{
    while (false == nodes.empty())
    {
        if (g_termination_handled)
            break;

        for (size_t node_index = nodes.size() - 1;
             node_index < nodes.size();
             --node_index)
        {
            auto& info = nodes[node_index];

            try
            {
                bool event_check = true;
                bool stop_check = false;

                // allow each node read all waiting
                // packets from network
                while (!stop_check && (event_check || info.peh->read()))
                    event_check = info.node->run(stop_check);

                if (stop_check)
                {
                    cout << endl << "Stop node : " << info.node->name();
                    nodes.erase(nodes.begin() + node_index);
                }
            }
            catch (std::bad_alloc const& ex)
            {
                if (info.plogger_exceptions)
                    info.plogger_exceptions->message(ex.what());
                cout << "exception cought: " << ex.what() << endl;
                cout << "will exit now" << endl;
                termination_handler(0);
                break;
            }
            catch (std::logic_error const& ex)
            {
                if (info.plogger_exceptions)
                    info.plogger_exceptions->message(ex.what());
                cout << "logic error cought: " << ex.what() << endl;
                cout << "will exit now" << endl;
                termination_handler(0);
                break;
            }
            catch (std::exception const& ex)
            {
                if (info.plogger_exceptions)
                    info.plogger_exceptions->message(ex.what());
                cout << "exception cought: " << ex.what() << endl;
            }
            catch (...)
            {
                if (info.plogger_exceptions)
                    info.plogger_exceptions->message("always throw std::exceptions, will exit now");
                cout << "always throw std::exceptions, will exit now" << endl;
                termination_handler(0);
                break;
            }
        }
    }

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
    //

#ifdef B_OS_WINDOWS
    signal(SIGINT, termination_handler);
#else
    struct sigaction signal_handler;
    signal_handler.sa_handler = termination_handler;
    ::sigaction(SIGINT, &signal_handler, nullptr);
    ::sigaction(SIGTERM, &signal_handler, nullptr);
#endif

//#define ATTACH
#ifdef ATTACH
    // just for debug
    int d;
    std::cout << "Now you can attach debugger" << endl;
    std::cout << "Enter a number to continue ..." << endl;
    std::cin >> d;
#endif

    try
    {
        meshpp::settings::set_application_name("simulation_publiqd");
        meshpp::create_config_directory();

        size_t node_count = 16;
        uint32_t connect_base = 10;
        string data_directory_root;

        if (false == process_command_line(argc, argv,
                                          data_directory_root,
                                          node_count,
                                          connect_base))
            return 1;

        boost::filesystem::path root = data_directory_root;
        boost::filesystem::create_directory(root);

        boost::filesystem::path state_file_path = root.string() + "/" + "state.txt";
        boost::filesystem::ofstream file_temp_state(state_file_path);

        network_simulation ns;
        ns.node_count = node_count;
        ns.chance_of_connect_base = connect_base;
        std::vector<vector<node_info>> nodes_info;

        size_t thread_count = std::thread::hardware_concurrency();

        if (0 == thread_count) // value is not well defined or not computable
            thread_count = 8;

        size_t node_counter = 0;
        size_t nodes_per_thread = node_count / thread_count;

        bool one_thread = (node_count >= thread_count);
        assert (true == one_thread);
        if (false == one_thread)
            throw std::logic_error("node_count <= thread_count");

        bool nodes_remainder = (node_count % thread_count == 0);
        assert (true == nodes_remainder);
        if (false == nodes_remainder)
            throw std::logic_error("node_count % thread_count != 0");

        nodes_info.resize(thread_count);

        for (size_t thread_index = 0; thread_index != thread_count; ++thread_index)
        {
            for (size_t node_index = 0; node_index != nodes_per_thread; ++node_index)
            {
                nodes_info[thread_index].resize(nodes_per_thread);

                node_info& info = nodes_info[thread_index][node_index];

                info.data_dir = (root / ("node_" + std::to_string(thread_index) + std::to_string(node_index))).string();

                meshpp::settings::set_data_directory(info.data_dir);
                meshpp::create_data_directory();

                info.config.set_data_directory(meshpp::settings::data_directory());

                string current_ip_address;
                if (0 == thread_index &&
                    0 == node_index)
                    current_ip_address = "test.brdhub.com";
                else
                    current_ip_address = std::to_string((node_counter / 250 / 250 / 250) % 250) + "." +
                                         std::to_string((node_counter / 250 / 250) % 250) + "." +
                                         std::to_string((node_counter / 250) % 250) + "." +
                                         std::to_string(node_counter % 250);

                ++node_counter;

                beltpp::ip_address p2p_bind_to_address;
                p2p_bind_to_address.local.address = current_ip_address;
                p2p_bind_to_address.local.port = 14500;

                bool resync = false;

                if (0 == thread_index &&
                    0 == node_index)
                    info.config.set_discovery_server();
                info.config.set_testnet();

                if (info.config.testnet())
                    meshpp::config::set_public_key_prefix("TPBQ");
                else
                    meshpp::config::set_public_key_prefix("PBQ");

                info.config.set_p2p_bind_to_address(p2p_bind_to_address);

                vector<beltpp::ip_address> p2p_connect_to_addresses;
                beltpp::ip_address p2p_connect_to_address;
                p2p_connect_to_address.from_string("test.brdhub.com:14500");
                p2p_connect_to_addresses.push_back(p2p_connect_to_address);
                info.config.set_p2p_connect_to_addresses(p2p_connect_to_addresses);

                beltpp::ip_address rpc_bind_to_address;
                //rpc_bind_to_address.local.address = current_ip_address;
                //rpc_bind_to_address.local.port = 14501;

                info.config.set_rpc_bind_to_address(rpc_bind_to_address);

                beltpp::ip_address slave_bind_to_address;
                info.config.set_slave_bind_to_address(slave_bind_to_address);

                beltpp::ip_address public_address;
                beltpp::ip_address public_ssl_address;
                info.config.set_public_address(public_address);
                info.config.set_public_ssl_address(public_ssl_address);
                info.config.set_automatic_fee(0);

                string manager_address;
                info.config.set_manager_address(manager_address);

                meshpp::random_seed seed;
                meshpp::private_key pv_key = seed.get_private_key(0);
                if (info.config.is_key_set())
                    pv_key = info.config.get_key();
                else
                    info.config.set_key(pv_key);

                info.config.set_node_type(string());    //  default is blockchain
                uint64_t freeze_before_block = uint64_t(-1);
                uint64_t revert_blocks_count = 0;
                uint64_t revert_actions_count = 0;

                //if (info.config.testnet() && 0 == node_index)
                //    pv_key = meshpp::private_key("5Kfu9216aabe2L942As4mGm91MC5RJKHP9tLWr5MMwcgVcRjFuz");

                string config_error = info.config.check_for_error();
                if (false == config_error.empty())
                {
                    cout << config_error << endl;
                    return 1;
                }

                info.dda.reset(new DataDirAttributeLoader(meshpp::data_file_path("running.txt")));
                {
                    PidConfig::RunningDuration item;
                    item.start.tm = item.end.tm = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

                    (*info.dda)->history.push_back(item);
                    (*info.dda).save();
                }

                auto fs_blockchain = meshpp::data_directory_path("blockchain");
                auto fs_action_log = meshpp::data_directory_path("action_log");
                auto fs_transaction_pool = meshpp::data_directory_path("transaction_pool");
                auto fs_state = meshpp::data_directory_path("state");
                auto fs_log = meshpp::data_directory_path("log");
                auto fs_documents = meshpp::data_directory_path("documents");
                auto fs_storages = meshpp::data_directory_path("storages");
                auto fs_storage = meshpp::data_directory_path("storage");
                auto fs_inbox = meshpp::data_directory_path("inbox");

                if (0 == thread_index &&
                    0 == node_index)
                {
                    info.plogger_p2p = beltpp::console_logger("publiqd_p2p", true);
                    info.plogger_rpc = beltpp::console_logger("publiqd_rpc", true);
                }
                else
                {
                    info.plogger_p2p = meshpp::file_logger("publiqd_p2p", fs_log / "publiqd_p2p.txt");
                    info.plogger_rpc = meshpp::file_logger("publiqd_rpc", fs_log / "publiqd_rpc.txt");
                }

                info.plogger_p2p->disable();
                info.plogger_rpc->disable();

                info.plogger_exceptions = meshpp::file_logger("publiqd_exceptions", fs_log / "exceptions.txt");

                event_handler_ns* peh = new event_handler_ns(ns);
                unique_ptr<beltpp::event_handler> inject_eh(peh);
                unique_ptr<beltpp::socket> inject_rpc_socket(new socket_ns(*peh,
                                                                           rpc_bind_to_address.local.address,
                                                                           "r" + format_index(node_counter, node_count)));
                unique_ptr<beltpp::socket> inject_p2p_socket(new socket_ns(*peh,
                                                                           p2p_bind_to_address.local.address,
                                                                           /*"p" +*/ format_index(node_counter, node_count)));

                info.peh = peh;
                info.node.reset(new publiqpp::node(
                    genesis_signed_block(info.config.testnet()),
                    fs_blockchain,
                    fs_action_log,
                    fs_transaction_pool,
                    fs_state,
                    fs_documents,
                    fs_storages,
                    fs_storage,
                    fs_inbox,
                    info.plogger_p2p.get(),
                    info.plogger_rpc.get(),
                    info.config,
                    freeze_before_block,
                    revert_blocks_count,
                    revert_actions_count,
                    resync,
                    mine_amount_threshhold(),
                    block_reward_array(),
                    &counts_per_channel_views,
                    &content_unit_validate_check,
                    std::move(inject_eh),
                    std::move(inject_rpc_socket),
                    std::move(inject_p2p_socket)));

                cout << "Node: " << node_counter << "  " << info.node->name() << " ";
                cout << "Type: " << BlockchainMessage::to_string(info.config.get_node_type()) << endl;

            } // for that initializes nodes
        }   //  iterate over threads

        std::vector<std::thread> threads(thread_count);

        for (size_t thread_index = 0; thread_index != thread_count; ++thread_index)
            threads.push_back(std::thread(run_nodes,
                                          std::ref(nodes_info[thread_index])));

        size_t cnt = 0;
        size_t step = 0;
        size_t sleep = 1;
        string step_str;
        time_point step_time;
        string connection_state;
        chrono::seconds step_duration;

        size_t realized_threads = 0;

        while (true)
        {
            for (size_t thread_index = 0; thread_index != thread_count; ++thread_index)
                if (true == nodes_info[thread_index].empty())
                    ++realized_threads;

            if (realized_threads == thread_count)
                break; // while (true)
            else
            {
                ns.process_attempts();
                //file_temp_state << ns.export_packets(beltpp::stream_join::rtt);

                ++step;
                time_point tmp_time = system_clock::now();
                step_duration = chrono::duration_cast<chrono::seconds>(tmp_time - step_time);

                // print network info
                //string tmp_state = ns.export_connections();
                string tmp_state = ns.export_connections_matrix();
                //string tmp_state = ns.export_connections_load();
                //string tmp_state = ns.export_counter();
                //string tmp_state = ns.export_network();
                string info = ns.export_connections_info();

                if (tmp_state != connection_state)
                {
                    cout << endl << endl;
                    cout << tmp_state << endl;
                    cout << info << endl << std::flush;

                    cnt = 0;

                    step_str = "Step " + std::to_string(step) + "   :   ";
                    step_str += " stable " + std::to_string(cnt) + "   :   ";
                    step_str += std::to_string(step_duration.count() - sleep) + " sec...  ";

                    cout << step_str << std::flush;

                    connection_state = tmp_state;
                }
                else
                {
                    cout << string(step_str.length(), '\b') << std::flush;

                    ++cnt;
                    step_str = "Step " + std::to_string(step) + "   :   ";
                    step_str += " stable " + std::to_string(cnt) + "   :   ";
                    step_str += std::to_string(step_duration.count() - sleep) + " sec...  ";

                    cout << step_str << std::flush;
                }

                step_time = tmp_time;
                std::this_thread::sleep_for(std::chrono::seconds(sleep));
            }
        } // while (true)

        for (auto& threads : nodes_info)
        {
            for (auto& node : threads)
            {
                (*node.dda)->history.back().end.tm = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                node.dda->save();
            }
        }

        for (auto& thread : threads)
            if(thread.joinable())
                thread.join();

    }
    catch (std::exception const& ex)
    {
        cout << "exception cought: " << ex.what() << endl;
    }
    catch (...)
    {
        cout << "always throw std::exceptions" << endl;
    }

    cout << "quit." << endl;

    return 0;
}

bool process_command_line(int argc, char** argv,
                          string& data_directory_root,
                          size_t& node_count,
                          uint32_t& connect_base)
{
    program_options::options_description options_description;
    try
    {
        auto desc_init = options_description.add_options()
                             ("help,h", "Print this help message and exit.")
                                 ("data_directory,d", program_options::value<string>(&data_directory_root), "Data directory path")
                                     ("nodes_count,n", program_options::value<size_t>(&node_count), "Nodes count")
                                         ("connect_base,c", program_options::value<uint32_t>(&connect_base), "Chance of connect base");
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


string genesis_signed_block(bool testnet)
{
#if 0
    Block genesis_block_mainnet;
    genesis_block_mainnet.header.block_number = 0;
    genesis_block_mainnet.header.delta = 0;
    genesis_block_mainnet.header.c_sum = 0;
    genesis_block_mainnet.header.c_const = 1;
    genesis_block_mainnet.header.prev_hash = meshpp::hash("PUBLIQ. blockchain distributed media. GETTING STARTED ON PUBLIQ. 26 March, 2019. https://publiq.network/en/gettin-started-on-publiq/");
    beltpp::gm_string_to_gm_time_t("2019-04-01 00:00:00", genesis_block_mainnet.header.time_signed.tm);

    string prefix = meshpp::config::public_key_prefix();
    Reward reward_publiq1;
    reward_publiq1.amount.whole = 150000000;
    reward_publiq1.reward_type = RewardType::initial;
    reward_publiq1.to = prefix + "7cGUNdApH4e958Nbj9WfEwmcjLUsFk88tz6TJNGtNuJ6WXRiKz";

    Reward reward_publiq2;
    reward_publiq2.amount.whole = 60000000;
    reward_publiq2.reward_type = RewardType::initial;
    reward_publiq2.to = prefix + "7VVS2JvqardqQ3hvGV8ANfHEQNn2SxC3RHKoHkGmzx8moRjFHy";

    Reward reward_publiq3;
    reward_publiq3.amount.whole = 40000000;
    reward_publiq3.reward_type = RewardType::initial;
    reward_publiq3.to = prefix + "7rnCF7htZsQmChm8dMm8eL7hoJMoTEnJqQheEbHKWBBKeZAibM";

    Reward reward_publiq_test1;
    reward_publiq_test1.amount.whole = 10000;
    reward_publiq_test1.reward_type = RewardType::initial;
    reward_publiq_test1.to = prefix + "7Ta31VaxCB9VfDRvYYosKYpzxXNgVH46UkM9i4FhzNg4JEU3YJ";

    Reward reward_publiq_test2;
    reward_publiq_test2.amount.whole = 10000;
    reward_publiq_test2.reward_type = RewardType::initial;
    reward_publiq_test2.to = prefix + "76Zv5QceNSLibecnMGEKbKo3dVFV6HRuDSuX59mJewJxHPhLwu";

    Reward reward_publiq_test3;
    reward_publiq_test3.amount.whole = 10000;
    reward_publiq_test3.reward_type = RewardType::initial;
    reward_publiq_test3.to = prefix + "8f5Z8SKVrYFES1KLHtCYMx276a5NTgZX6baahzTqkzfnB4Pidk";

    Reward reward_publiq_test4;
    reward_publiq_test4.amount.whole = 10000;
    reward_publiq_test4.reward_type = RewardType::initial;
    reward_publiq_test4.to = prefix + "8MiwBdYzSj38etLYLES4FSuKJnLPkXAJv4MyrLW7YJNiPbh4z6";

    genesis_block_mainnet.rewards =
    {
        reward_publiq1,
        reward_publiq2,
        reward_publiq3
    };

    Block genesis_block_testnet = genesis_block_mainnet;
    genesis_block_testnet.rewards =
    {
        reward_publiq1,
        reward_publiq2,
        reward_publiq3,
        reward_publiq_test1,
        reward_publiq_test2,
        reward_publiq_test3,
        reward_publiq_test4
    };

    meshpp::random_seed seed;
    meshpp::private_key pvk = seed.get_private_key(0);
    meshpp::public_key pbk = pvk.get_public_key();

    SignedBlock sb;
    if (testnet)
        sb.block_details = std::move(genesis_block_testnet);
    else
        sb.block_details = std::move(genesis_block_mainnet);

    Authority authorization;
    authorization.address = pbk.to_string();
    authorization.signature = pvk.sign(sb.block_details.to_string()).base58;

    sb.authorization = authorization;

    std::cout << sb.to_string() << std::endl;
#endif
    std::string str_genesis_testnet = R"genesis(
                                      {
                                         "rtt":8,
                                         "block_details":{
                                            "rtt":7,
                                            "header":{
                                               "rtt":5,
                                               "block_number":0,
                                               "delta":0,
                                               "c_sum":0,
                                               "c_const":1,
                                               "prev_hash":"GnFozpoEMEuVkVFnuJfVn64oWqpPmYQSyzsmo7ptmUD3",
                                               "time_signed":"2019-04-01 00:00:00"
                                            },
                                            "rewards":[
                                               {
                                                  "rtt":12,
                                                  "to":"TPBQ7cGUNdApH4e958Nbj9WfEwmcjLUsFk88tz6TJNGtNuJ6WXRiKz",
                                                  "amount":{
                                                     "rtt":0,
                                                     "whole":150000000,
                                                     "fraction":0
                                                  },
                                                  "reward_type":"initial"
                                               },
                                               {
                                                  "rtt":12,
                                                  "to":"TPBQ7VVS2JvqardqQ3hvGV8ANfHEQNn2SxC3RHKoHkGmzx8moRjFHy",
                                                  "amount":{
                                                     "rtt":0,
                                                     "whole":60000000,
                                                     "fraction":0
                                                  },
                                                  "reward_type":"initial"
                                               },
                                               {
                                                  "rtt":12,
                                                  "to":"TPBQ7rnCF7htZsQmChm8dMm8eL7hoJMoTEnJqQheEbHKWBBKeZAibM",
                                                  "amount":{
                                                     "rtt":0,
                                                     "whole":40000000,
                                                     "fraction":0
                                                  },
                                                  "reward_type":"initial"
                                               },
                                               {
                                                  "rtt":12,
                                                  "to":"TPBQ7Ta31VaxCB9VfDRvYYosKYpzxXNgVH46UkM9i4FhzNg4JEU3YJ",
                                                  "amount":{
                                                     "rtt":0,
                                                     "whole":10000,
                                                     "fraction":0
                                                  },
                                                  "reward_type":"initial"
                                               },
                                               {
                                                  "rtt":12,
                                                  "to":"TPBQ76Zv5QceNSLibecnMGEKbKo3dVFV6HRuDSuX59mJewJxHPhLwu",
                                                  "amount":{
                                                     "rtt":0,
                                                     "whole":10000,
                                                     "fraction":0
                                                  },
                                                  "reward_type":"initial"
                                               },
                                               {
                                                  "rtt":12,
                                                  "to":"TPBQ8f5Z8SKVrYFES1KLHtCYMx276a5NTgZX6baahzTqkzfnB4Pidk",
                                                  "amount":{
                                                     "rtt":0,
                                                     "whole":10000,
                                                     "fraction":0
                                                  },
                                                  "reward_type":"initial"
                                               },
                                               {
                                                  "rtt":12,
                                                  "to":"TPBQ8MiwBdYzSj38etLYLES4FSuKJnLPkXAJv4MyrLW7YJNiPbh4z6",
                                                  "amount":{
                                                     "rtt":0,
                                                     "whole":10000,
                                                     "fraction":0
                                                  },
                                                  "reward_type":"initial"
                                               }
                                            ],
                                            "signed_transactions":[

                                            ]
                                         },
                                         "authorization":{
                                            "rtt":3,
                                            "address":"TPBQ7ztjFZ28yCrcYffMEYQp3kMtRV1g5QmgiSkp4d7ic4FU2BdUFd",
                                            "signature":"381yXZB2msTUmsnhZVXus7YwhWSbwbbvnBS4JpK1cnVqrjsEnqHL9FBYN9GJeECHfbfE6aGs7awn7W1jeegSMrHAEwAhoxPh"
                                         }
                                      }
                                      )genesis";
    std::string str_genesis_mainnet = R"genesis(
                                      {
                                         "rtt":8,
                                         "block_details":{
                                            "rtt":7,
                                            "header":{
                                               "rtt":5,
                                               "block_number":0,
                                               "delta":0,
                                               "c_sum":0,
                                               "c_const":1,
                                               "prev_hash":"GnFozpoEMEuVkVFnuJfVn64oWqpPmYQSyzsmo7ptmUD3",
                                               "time_signed":"2019-04-01 00:00:00"
                                            },
                                            "rewards":[
                                               {
                                                  "rtt":12,
                                                  "to":"PBQ7cGUNdApH4e958Nbj9WfEwmcjLUsFk88tz6TJNGtNuJ6WXRiKz",
                                                  "amount":{
                                                     "rtt":0,
                                                     "whole":150000000,
                                                     "fraction":0
                                                  },
                                                  "reward_type":"initial"
                                               },
                                               {
                                                  "rtt":12,
                                                  "to":"PBQ7VVS2JvqardqQ3hvGV8ANfHEQNn2SxC3RHKoHkGmzx8moRjFHy",
                                                  "amount":{
                                                     "rtt":0,
                                                     "whole":60000000,
                                                     "fraction":0
                                                  },
                                                  "reward_type":"initial"
                                               },
                                               {
                                                  "rtt":12,
                                                  "to":"PBQ7rnCF7htZsQmChm8dMm8eL7hoJMoTEnJqQheEbHKWBBKeZAibM",
                                                  "amount":{
                                                     "rtt":0,
                                                     "whole":40000000,
                                                     "fraction":0
                                                  },
                                                  "reward_type":"initial"
                                               }
                                            ],
                                            "signed_transactions":[

                                            ]
                                         },
                                         "authorization":{
                                            "rtt":3,
                                            "address":"PBQ6sx9qUobi84gcEDxtHqbEY1HScyiBQ3HodnskwfVf1RYkU77Za",
                                            "signature":"AN1rKp6KAXTNXpAbC5SzpWAbs2KVvoxVGjGdkED5JJgUZfh2vhALctCo4s3x9y7dCcpKgE8dEfSDRGtaRy7MfzN4PRqQYCiEb"
                                         }
                                      }
                                      )genesis";

    if (testnet)
        return str_genesis_testnet;
    else
        return str_genesis_mainnet;
}

publiqpp::coin mine_amount_threshhold()
{
    return publiqpp::coin(10000, 0);
}

vector<publiqpp::coin> block_reward_array()
{
    using coin = publiqpp::coin;
    return vector<publiqpp::coin>
        {
            coin(1000,0),     coin(800,0),      coin(640,0),        coin(512,0),        coin(410,0),        coin(327,0),
            coin(262,0),      coin(210,0),      coin(168,0),        coin(134,0),        coin(107,0),        coin(86,0),
            coin(68,0),       coin(55,0),       coin(44,0),         coin(35,0),         coin(28,0),         coin(22,0),
            coin(18,0),       coin(15,0),       coin(12,0),         coin(9,0),          coin(7,0),          coin(6,0),
            coin(5,0),        coin(4,0),        coin(3,0),          coin(2,50000000),   coin(2,0),          coin(1,50000000),
            coin(1,20000000), coin(1,0),        coin(0,80000000),   coin(0,70000000),   coin(0,60000000),   coin(0,50000000),
            coin(0,40000000), coin(0,30000000), coin(0,20000000),   coin(0,17000000),   coin(0,14000000),   coin(0,12000000),
            coin(0,10000000), coin(0,8000000),  coin(0,7000000),    coin(0,6000000),    coin(0,6000000),    coin(0,5000000),
            coin(0,5000000),  coin(0,5000000),  coin(0,4000000),    coin(0,4000000),    coin(0,4000000),    coin(0,4000000),
            coin(0,4000000),  coin(0,3000000),  coin(0,3000000),    coin(0,3000000),    coin(0,3000000),    coin(0,3000000)
        };
}