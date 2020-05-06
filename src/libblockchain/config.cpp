#include "config.hpp"
#include "common.hpp"
#include "message.tmpl.hpp"

#include <mesh.pp/settings.hpp>
#include <mesh.pp/fileutility.hpp>

#include <boost/filesystem.hpp>

#include <algorithm>

using std::string;
namespace filesystem =  boost::filesystem;
using meshpp::public_key;
using meshpp::private_key;
using std::vector;
using beltpp::ip_address;

namespace publiqpp
{
namespace detail
{
class config_internal
{
public:
    meshpp::file_loader<BlockchainMessage::Config,
                        &BlockchainMessage::Config::from_string,
                        &BlockchainMessage::Config::to_string> config_loader;

    config_internal(filesystem::path const& data_directory)
        : config_loader(data_directory / ("config.json"))
    {}
};

}   //  end detail

config::config()
    : pimpl(nullptr)
{}

config::~config() = default;

void config::set_data_directory(string const& str_data_directory)
{
    filesystem::path data_directory(str_data_directory);
    pimpl.reset(new detail::config_internal(data_directory));
}

void config::set_p2p_bind_to_address(ip_address const& address)
{
    if (false == address.local.empty())
    {
        if (!pimpl->config_loader->p2p_bind_to_address)
            pimpl->config_loader->p2p_bind_to_address = BlockchainMessage::IPAddress();
        beltpp::assign(*pimpl->config_loader->p2p_bind_to_address, address);

        pimpl->config_loader.save();
        pimpl->config_loader.commit();
    }
}

ip_address config::get_p2p_bind_to_address() const
{
    ip_address result;
    if (pimpl->config_loader->p2p_bind_to_address)
        beltpp::assign(result, *pimpl->config_loader->p2p_bind_to_address);
    return result;
}

void config::set_p2p_connect_to_addresses(vector<ip_address> const& addresses)
{
    if (false == addresses.empty())
    {
        auto& storage_addresses = pimpl->config_loader->p2p_connect_to_addresses;
        bool cleared = false;

        for (auto address : addresses)
        {
            if (false == address.local.empty() &&
                address.remote.empty())
                address.remote = address.local;

            address.local = beltpp::ip_destination();

            if (address.remote.empty())
                continue;

            BlockchainMessage::IPAddress item;
            beltpp::assign(item, address);

            if (!storage_addresses)
                storage_addresses = vector<BlockchainMessage::IPAddress>();

            if (false == cleared)
            {
                storage_addresses->clear();
                cleared = true;
            }
            storage_addresses->push_back(item);
        }

        pimpl->config_loader.save();
        pimpl->config_loader.commit();
    }
}

vector<ip_address> config::get_p2p_connect_to_addresses() const
{
    vector<ip_address> addresses;

    auto& storage_addresses = pimpl->config_loader->p2p_connect_to_addresses;

    if (storage_addresses)
    for (auto const& address : *storage_addresses)
    {
        ip_address item;
        beltpp::assign(item, address);

        addresses.push_back(item);
    }

    return addresses;
}

void config::set_rpc_bind_to_address(ip_address const& address)
{
    if (false == address.local.empty())
    {
        if (!pimpl->config_loader->rpc_bind_to_address)
            pimpl->config_loader->rpc_bind_to_address = BlockchainMessage::IPAddress();
        beltpp::assign(*pimpl->config_loader->rpc_bind_to_address, address);

        pimpl->config_loader.save();
        pimpl->config_loader.commit();
    }
}

ip_address config::get_rpc_bind_to_address() const
{
    ip_address result;
    if (pimpl->config_loader->rpc_bind_to_address)
        beltpp::assign(result, *pimpl->config_loader->rpc_bind_to_address);
    return result;
}

void config::set_slave_bind_to_address(ip_address const& address)
{
    if (false == address.local.empty())
    {
        if (!pimpl->config_loader->slave_bind_to_address)
            pimpl->config_loader->slave_bind_to_address = BlockchainMessage::IPAddress();
        beltpp::assign(*pimpl->config_loader->slave_bind_to_address, address);

        pimpl->config_loader.save();
        pimpl->config_loader.commit();
    }
}

ip_address config::get_slave_bind_to_address() const
{
    ip_address result;
    if (pimpl->config_loader->slave_bind_to_address)
        beltpp::assign(result, *pimpl->config_loader->slave_bind_to_address);
    return result;
}

void config::set_public_address(ip_address const& address)
{
    if (false == address.local.empty())
    {
        if (!pimpl->config_loader->public_address)
            pimpl->config_loader->public_address = BlockchainMessage::IPAddress();
        beltpp::assign(*pimpl->config_loader->public_address, address);

        pimpl->config_loader.save();
        pimpl->config_loader.commit();
    }
}

ip_address config::get_public_address() const
{
    ip_address result;
    if (pimpl->config_loader->public_address)
        beltpp::assign(result, *pimpl->config_loader->public_address);
    return result;
}

void config::set_public_ssl_address(ip_address const& address)
{
    if (false == address.local.empty())
    {
        if (!pimpl->config_loader->public_ssl_address)
            pimpl->config_loader->public_ssl_address = BlockchainMessage::IPAddress();
        beltpp::assign(*pimpl->config_loader->public_ssl_address, address);

        pimpl->config_loader.save();
        pimpl->config_loader.commit();
    }
}

ip_address config::get_public_ssl_address() const
{
    ip_address result;
    if (pimpl->config_loader->public_ssl_address)
        beltpp::assign(result, *pimpl->config_loader->public_ssl_address);
    return result;
}

void config::set_manager_address(string const& manager_address)
{
    if (false == manager_address.empty())
    {
        pimpl->config_loader->manager_address = manager_address;

        pimpl->config_loader.save();
        pimpl->config_loader.commit();
    }
}
string config::get_manager_address() const
{
    return pimpl->config_loader->manager_address.value_or(string());
}

void config::set_key(const private_key &pk)
{
    pimpl->config_loader->private_key = pk.get_base58_wif();

    pimpl->config_loader.save();
    pimpl->config_loader.commit();
}

private_key config::get_key() const
{
    if (pimpl->config_loader->private_key)
        return private_key(*pimpl->config_loader->private_key);

    throw std::logic_error("config::get_key");
}
bool config::key_set() const
{
    if (pimpl->config_loader->private_key)
        return true;
    return false;
}

void config::add_secondary_key(private_key const& pk)
{
    if (key_set() &&
        get_key().get_base58_wif() == pk.get_base58_wif())
        return;

    auto& pks = pimpl->config_loader->private_keys;
    if (pks &&
        pks->end() != std::find(pks->begin(), pks->end(), pk.get_base58_wif()))
        return;

    if (!pks)
        pks = vector<string>();

    pks->push_back(pk.get_base58_wif());

    pimpl->config_loader.save();
    pimpl->config_loader.commit();
}

void config::remove_secondary_key(private_key const& pk)
{
    auto& pks = pimpl->config_loader->private_keys;
    if (!pks)
        return;

    pks->erase(std::remove_if(pks->begin(),
                              pks->end(),
                              [&pk](string const& value) { return value == pk.get_base58_wif(); }),
               pks->end());

    pimpl->config_loader.save();
    pimpl->config_loader.commit();
}

std::vector<private_key> config::keys() const
{
    std::vector<private_key> result;
    result.push_back(get_key());

    if (pimpl->config_loader->private_keys)
    for (auto const& pkitem : *pimpl->config_loader->private_keys)
        result.push_back(private_key(pkitem));

    return result;
}

void config::set_node_type(string const& node_type)
{
    if (false == node_type.empty())
    {
        BlockchainMessage::NodeType temp;
        BlockchainMessage::from_string(node_type, temp);
        pimpl->config_loader->node_type = temp;

        pimpl->config_loader.save();
        pimpl->config_loader.commit();
    }
}

BlockchainMessage::NodeType config::get_node_type() const
{
    return pimpl->config_loader->node_type.value_or(BlockchainMessage::NodeType::blockchain);
}

void config::set_automatic_fee(size_t fractions)
{
    if (0 != fractions)
    {
        coin result(0, 1);
        result *= fractions;

        if (!pimpl->config_loader->automatic_fee)
            pimpl->config_loader->automatic_fee = BlockchainMessage::Coin();

        pimpl->config_loader.save();
        pimpl->config_loader.commit();
    }
}

coin config::get_automatic_fee() const
{
    coin result;

    if (pimpl->config_loader->automatic_fee)
        result = coin(*pimpl->config_loader->automatic_fee);

    return result;
}

void config::enable_action_log()
{
    pimpl->config_loader->enable_action_log = true;

    pimpl->config_loader.save();
    pimpl->config_loader.commit();
}

bool config::action_log() const
{
    if (pimpl->config_loader->enable_action_log)
        return *pimpl->config_loader->enable_action_log;

    return false;
}

void config::enable_inbox()
{
    pimpl->config_loader->enable_inbox = true;

    pimpl->config_loader.save();
    pimpl->config_loader.commit();
}

bool config::inbox() const
{
    if (pimpl->config_loader->enable_inbox)
        return *pimpl->config_loader->enable_inbox;

    return false;
}

void config::set_testnet()
{
    pimpl->config_loader->testnet = true;

    pimpl->config_loader.save();
    pimpl->config_loader.commit();
}

bool config::testnet() const
{
    if (pimpl->config_loader->testnet)
        return *pimpl->config_loader->testnet;

    return false;
}

void config::set_transfer_only()
{
    pimpl->config_loader->transfer_only = true;

    pimpl->config_loader.save();
    pimpl->config_loader.commit();
}

bool config::transfer_only() const
{
    if (pimpl->config_loader->transfer_only)
        return *pimpl->config_loader->transfer_only;

    return false;
}

void config::set_discovery_server()
{
    pimpl->config_loader->discovery_server = true;

    pimpl->config_loader.save();
    pimpl->config_loader.commit();
}

bool config::discovery_server() const
{
    if (pimpl->config_loader->discovery_server)
        return *pimpl->config_loader->discovery_server;

    return false;
}

string config::check_for_error() const
{
    string result;

    if (get_p2p_bind_to_address().local.empty())
        result = "p2p_local_interface is mandatory";
    if (get_node_type() == BlockchainMessage::NodeType::blockchain &&
        false == get_public_address().local.empty() &&
        get_rpc_bind_to_address().local.empty())
        result = "rpc_local_interface is not specified";
    else if (get_node_type() != BlockchainMessage::NodeType::blockchain &&
             get_public_address().local.empty())
        result = "public_address is not specified";
    else if (get_node_type() != BlockchainMessage::NodeType::blockchain &&
             get_slave_bind_to_address().local.empty())
        result = "slave_local_interface is not specified";

    if (false == result.empty())
        result += ". run with --help to see more detals.";

    return result;
}

}   //  end publiqpp