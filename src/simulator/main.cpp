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
#include <chrono>
#include <map>
#include <unordered_set>

#include <csignal>

using namespace BlockchainMessage;
namespace program_options = boost::program_options;

using std::unique_ptr;
using std::string;
using std::unordered_set;
using std::cout;
using std::endl;
using std::vector;
using std::runtime_error;

string genesis_signed_block(bool testnet);
publiqpp::coin mine_amount_threshhold();
vector<publiqpp::coin> block_reward_array();

using DataDirAttributeLoader = meshpp::file_locker<meshpp::file_loader<PidConfig::DataDirAttribute,
                                                                       &PidConfig::DataDirAttribute::from_string,
                                                                       &PidConfig::DataDirAttribute::to_string>>;

class port2pid_helper
{
    using Loader = meshpp::file_locker<meshpp::file_loader<PidConfig::Port2PID,
                                                           &PidConfig::Port2PID::from_string,
                                                           &PidConfig::Port2PID::to_string>>;
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

static bool g_termination_handled = false;
static vector<publiqpp::node*> g_pnodes;
static vector<publiqpp::storage_node*> g_pstorage_nodes;

void termination_handler(int /*signum*/)
{
    cout << "stopping..." << endl;

    g_termination_handled = true;
    for(auto g_pnode : g_pnodes)
        g_pnode->wake();
    for (auto g_pstorage_node : g_pstorage_nodes)
        g_pstorage_node->wake();
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

template <typename NODE>
void loop(NODE& node, /*beltpp::ilog_ptr& plogger_exceptions,*/ bool& termination_handled);

int main()
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

    try
    {
        for (int port = 18885; port < 18890; ++port)
        {
            meshpp::settings::set_application_name("publiqd");
            meshpp::settings::set_data_directory(meshpp::config_directory_path().string() + std::to_string(port));

            beltpp::ip_address p2p_bind_to_address;
            p2p_bind_to_address.from_string("127.0.0.1:" + std::to_string(port));

            vector<beltpp::ip_address> p2p_connect_to_addresses;
            beltpp::ip_address connect_to_address;
            connect_to_address.from_string("127.0.0.1:" + std::to_string(port - 1));
            p2p_connect_to_addresses.push_back(connect_to_address);

            beltpp::ip_address rpc_bind_to_address;
            beltpp::ip_address slave_bind_to_address;
            beltpp::ip_address public_address;
            beltpp::ip_address public_ssl_address;

            string data_directory;

            NodeType n_type = BlockchainMessage::NodeType::blockchain;
            uint64_t fractions = 0;
            uint64_t freeze_before_block = uint64_t(-1);
            uint64_t revert_blocks_count = 0;
            uint64_t revert_actions_count = 0;
            string manager_address;
            bool log_enabled = true;
            bool testnet = true;
            bool resync = false;
            bool enable_inbox = true;
            bool discovery_server = false;
            meshpp::random_seed seed;
            meshpp::private_key pv_key = seed.get_private_key(0);

            if (testnet)
                meshpp::config::set_public_key_prefix("TPBQ");
            else
                meshpp::config::set_public_key_prefix("PBQ");

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

            beltpp::ilog_ptr plogger_exceptions = beltpp::t_unique_nullptr<beltpp::ilog>();
            beltpp::ilog_ptr plogger_storage_exceptions = beltpp::t_unique_nullptr<beltpp::ilog>();

                meshpp::create_config_directory();
                meshpp::create_data_directory();

                unique_ptr<port2pid_helper> port2pid(new port2pid_helper(meshpp::config_file_path("pid"), p2p_bind_to_address.local.port));

                DataDirAttributeLoader dda(meshpp::data_file_path("running.txt"));
                {
                    PidConfig::RunningDuration item;
                    item.start.tm = item.end.tm = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

                    dda->history.push_back(item);
                    dda.save();
                }

                auto fs_blockchain = meshpp::data_directory_path("blockchain");
                auto fs_action_log = meshpp::data_directory_path("action_log");
                auto fs_transaction_pool = meshpp::data_directory_path("transaction_pool");
                auto fs_state = meshpp::data_directory_path("state");
                auto fs_log = meshpp::data_directory_path("log");
                auto fs_documents = meshpp::data_directory_path("documents");
                auto fs_storages = meshpp::data_directory_path("storages");

                cout << "p2p local address: " << p2p_bind_to_address.to_string() << endl;
                for (auto const& item : p2p_connect_to_addresses)
                    cout << "p2p host: " << item.to_string() << endl;
                if (false == rpc_bind_to_address.local.empty())
                    cout << "rpc interface: " << rpc_bind_to_address.to_string() << endl;

                beltpp::ilog_ptr plogger_p2p = beltpp::console_logger("publiqd_p2p", true);
                plogger_p2p->disable();
                beltpp::ilog_ptr plogger_rpc = beltpp::console_logger("publiqd_rpc", true);
                plogger_rpc->disable();
                plogger_exceptions = meshpp::file_logger("publiqd_exceptions",
                                                         fs_log / "exceptions.txt");
                plogger_storage_exceptions = meshpp::file_logger("storage_exceptions",
                                                                 fs_log / "storage_exceptions.txt");

                //__debugbreak();
                boost::filesystem::path fs_storage;
                if (n_type == NodeType::storage)
                    fs_storage = meshpp::data_directory_path("storage");

                boost::filesystem::path fs_inbox;
                if (enable_inbox)
                    fs_inbox = meshpp::data_directory_path("inbox");

                publiqpp::node *node = new publiqpp::node(
                                    genesis_signed_block(testnet),
                                    public_address,
                                    public_ssl_address,
                                    rpc_bind_to_address,
                                    p2p_bind_to_address,
                                    p2p_connect_to_addresses,
                                    fs_blockchain,
                                    fs_action_log,
                                    fs_transaction_pool,
                                    fs_state,
                                    fs_documents,
                                    fs_storages,
                                    fs_storage,
                                    fs_inbox,
                                    plogger_p2p.get(),
                                    plogger_rpc.get(),
                                    pv_key,
                                    n_type,
                                    fractions,
                                    freeze_before_block,
                                    revert_blocks_count,
                                    revert_actions_count,
                                    manager_address,
                                    log_enabled,
                                    false,
                                    testnet,
                                    resync,
                                    discovery_server,
                                    mine_amount_threshhold(),
                                    block_reward_array(),
                                    &counts_per_channel_views,
                                    &content_unit_validate_check);

                cout << endl;
                cout << "Node: " << node->name() << endl;
                cout << "Type: " << static_cast<int>(n_type) << endl;
                cout << endl;

                g_pnodes.push_back(node);

                unique_ptr<publiqpp::storage_node> ptr_storage_node;
                if (n_type != NodeType::blockchain)
                {
                    fs_storage = meshpp::data_directory_path("storage");
                    ptr_storage_node.reset(new publiqpp::storage_node(*node,
                                                                      slave_bind_to_address,
                                                                      fs_storage,
                                                                      pv_key,
                                                                      plogger_rpc.get()));
                    g_pstorage_nodes.push_back(ptr_storage_node.get());
                }
    }

    while (false == g_termination_handled)
    {
        for (auto const& node : g_pnodes)
        {
            bool stop_check = false;
            node->run(stop_check);
            if (stop_check)
            {
                termination_handler(0);
                break;
            }
        }
    }

    //            dda->history.back().end.tm = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    //            dda.save();
    //            port2pid->commit();
    }
    catch (std::exception const& ex)
    {
//        if (plogger_exceptions)
//            plogger_exceptions->message(ex.what());
        cout << "exception cought: " << ex.what() << endl;
    }
    catch (...)
    {
//        if (plogger_exceptions)
//            plogger_exceptions->message("always throw std::exceptions");
        cout << "always throw std::exceptions" << endl;
    }

    cout << "quit." << endl;

    return 0;
}

template <typename NODE>
void loop(NODE& node, /*beltpp::ilog_ptr& plogger_exceptions,*/ bool& termination_handled)
{
    bool stop_check = false;
    while (false == stop_check)
    {
        try
        {
            if (termination_handled)
                break;
            node->run(stop_check);
            if (stop_check)
            {
                termination_handler(0);
                break;
            }
        }
        catch (std::bad_alloc const& ex)
        {
//            if (plogger_exceptions)
//                plogger_exceptions->message(ex.what());
            cout << "exception cought: " << ex.what() << endl;
            cout << "will exit now" << endl;
            termination_handler(0);
            break;
        }
        catch (std::logic_error const& ex)
        {
//            if (plogger_exceptions)
//                plogger_exceptions->message(ex.what());
            cout << "logic error cought: " << ex.what() << endl;
            cout << "will exit now" << endl;
            termination_handler(0);
            break;
        }
        catch (std::exception const& ex)
        {
//            if (plogger_exceptions)
//                plogger_exceptions->message(ex.what());
            cout << "exception cought: " << ex.what() << endl;
        }
        catch (...)
        {
//            if (plogger_exceptions)
//                plogger_exceptions->message("always throw std::exceptions, will exit now");
            cout << "always throw std::exceptions, will exit now" << endl;
            termination_handler(0);
            break;
        }
    }
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
