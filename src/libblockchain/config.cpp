#include "config.hpp"
#include "common.hpp"
#include "message.tmpl.hpp"

#include <mesh.pp/settings.hpp>
#include <mesh.pp/fileutility.hpp>
#include <mesh.pp/cryptoutility.hpp>

#include <boost/filesystem.hpp>

#include <algorithm>
#include <mutex>

using std::string;
namespace filesystem =  boost::filesystem;
using meshpp::public_key;
using meshpp::private_key;
using std::vector;
using beltpp::ip_address;
using std::mutex;
using std::unique_lock;

namespace publiqpp
{
namespace detail
{
class config_internal
{
public:
    mutex m_mutex;
    string aes_key;
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

config::config(config&&) noexcept = default;

config::~config() = default;

config& config::operator = (config&& other) noexcept = default;

void config::set_data_directory(string const& str_data_directory)
{
    filesystem::path data_directory(str_data_directory);
    pimpl.reset(new detail::config_internal(data_directory));
}

void config::set_aes_key(std::string const& key)
{
    auto locker = unique_lock<mutex>(pimpl->m_mutex);
    pimpl->aes_key = key;
}

void config::set_p2p_bind_to_address(ip_address const& address)
{
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

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
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

    ip_address result;
    if (pimpl->config_loader->p2p_bind_to_address)
        beltpp::assign(result, *pimpl->config_loader->p2p_bind_to_address);
    return result;
}

void config::set_p2p_connect_to_addresses(vector<ip_address> const& addresses)
{
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

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
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

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
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

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
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

    ip_address result;
    if (pimpl->config_loader->rpc_bind_to_address)
        beltpp::assign(result, *pimpl->config_loader->rpc_bind_to_address);
    return result;
}

void config::set_slave_bind_to_address(ip_address const& address)
{
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

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
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

    ip_address result;
    if (pimpl->config_loader->slave_bind_to_address)
        beltpp::assign(result, *pimpl->config_loader->slave_bind_to_address);
    return result;
}

void config::set_public_address(ip_address const& address)
{
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

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
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

    ip_address result;
    if (pimpl->config_loader->public_address)
        beltpp::assign(result, *pimpl->config_loader->public_address);
    return result;
}

void config::set_public_ssl_address(ip_address const& address)
{
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

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
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

    ip_address result;
    if (pimpl->config_loader->public_ssl_address)
        beltpp::assign(result, *pimpl->config_loader->public_ssl_address);
    return result;
}

void config::set_manager_address(string const& manager_address)
{
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

    if (false == manager_address.empty())
    {
        pimpl->config_loader->manager_address = manager_address;

        pimpl->config_loader.save();
        pimpl->config_loader.commit();
    }
}

string config::get_manager_address() const
{
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

    return pimpl->config_loader->manager_address.value_or(string());
}

void config::set_key(const private_key &pk)
{
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

    string encrypted_private_key =
            meshpp::aes_encrypt(pimpl->aes_key, pk.get_base58_wif());
    pimpl->config_loader->private_key = encrypted_private_key;

    pimpl->config_loader.save();
    pimpl->config_loader.commit();
}

private_key config::get_key() const
{
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

    auto& pk = pimpl->config_loader->private_key;
    if (pk)
    {
        string decrypted_private_key =
                meshpp::aes_decrypt(pimpl->aes_key,
                                    *pk);
        return private_key(decrypted_private_key);
    }

    throw std::logic_error("config::get_key");
}
bool config::is_key_set() const
{
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

    if (pimpl->config_loader->private_key)
        return true;
    return false;
}

void config::set_public_key(public_key const& pbk)
{
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

    pimpl->config_loader->public_key = pbk.to_string();

    pimpl->config_loader.save();
    pimpl->config_loader.commit();
}

public_key config::get_public_key() const
{
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

    auto& pbk = pimpl->config_loader->public_key;
    if (pbk)
        return public_key(*pbk);

    throw std::logic_error("config::get_public_key");
}

bool config::is_public_key_set() const
{
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

    if (pimpl->config_loader->public_key)
        return true;
    return false;
}

void config::add_secondary_key(private_key const& pk)
{
    if (is_key_set() &&
        get_key().get_base58_wif() == pk.get_base58_wif())
        return;

    string encrypted_private_key =
            meshpp::aes_encrypt(pimpl->aes_key, pk.get_base58_wif());

    auto locker = unique_lock<mutex>(pimpl->m_mutex);

    auto& pks = pimpl->config_loader->private_keys;
    if (pks &&
        pks->end() != std::find(pks->begin(), pks->end(), encrypted_private_key))
        return;

    if (!pks)
        pks = vector<string>();

    pks->push_back(encrypted_private_key);

    pimpl->config_loader.save();
    pimpl->config_loader.commit();
}

void config::remove_secondary_key(private_key const& pk)
{
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

    auto& pks = pimpl->config_loader->private_keys;
    if (!pks)
        return;

    string encrypted_private_key =
            meshpp::aes_encrypt(pimpl->aes_key, pk.get_base58_wif());

    pks->erase(std::remove_if(pks->begin(),
                              pks->end(),
                              [&encrypted_private_key](string const& value) { return value == encrypted_private_key; }),
               pks->end());

    pimpl->config_loader.save();
    pimpl->config_loader.commit();
}

std::vector<private_key> config::keys() const
{
    std::vector<private_key> result;
    result.push_back(get_key());

    auto locker = unique_lock<mutex>(pimpl->m_mutex);

    auto& pks = pimpl->config_loader->private_keys;

    if (pks)
    for (auto const& pkitem : *pks)
    {
        string decrypted_private_key =
                meshpp::aes_decrypt(pimpl->aes_key,
                                    pkitem);
        result.push_back(private_key(decrypted_private_key));
    }

    return result;
}

void config::add_secondary_public_key(public_key const& pbk)
{
    string pbk_string = pbk.to_string();

    if (is_public_key_set() &&
        get_public_key().to_string() == pbk_string)
        return;

    auto locker = unique_lock<mutex>(pimpl->m_mutex);

    auto& pbks = pimpl->config_loader->public_keys;
    if (pbks &&
        pbks->end() != std::find(pbks->begin(), pbks->end(), pbk_string))
        return;

    if (!pbks)
        pbks = vector<string>();

    pbks->push_back(pbk_string);

    pimpl->config_loader.save();
    pimpl->config_loader.commit();
}

void config::remove_secondary_public_key(public_key const& pbk)
{
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

    auto& pbks = pimpl->config_loader->public_keys;
    if (!pbks)
        return;

    string pbk_string = pbk.to_string();

    pbks->erase(std::remove_if(pbks->begin(),
                              pbks->end(),
                              [&pbk_string](string const& value) { return value == pbk_string; }),
               pbks->end());

    pimpl->config_loader.save();
    pimpl->config_loader.commit();
}

vector<public_key> config::public_keys() const
{
    vector<public_key> result;
    result.push_back(get_public_key());

    auto locker = unique_lock<mutex>(pimpl->m_mutex);

    auto& pbks = pimpl->config_loader->public_keys;

    if (pbks)
    for (auto const& pbkitem : *pbks)
        result.push_back(public_key(pbkitem));

    return result;
}

void config::set_node_type(string const& node_type)
{
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

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
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

    return pimpl->config_loader->node_type.value_or(BlockchainMessage::NodeType::blockchain);
}

void config::set_automatic_fee(size_t fractions)
{
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

    if (0 != fractions)
    {
        coin result(0, 1);
        result *= fractions;

        if (!pimpl->config_loader->automatic_fee)
            pimpl->config_loader->automatic_fee = BlockchainMessage::Coin();

        result.to_Coin(*pimpl->config_loader->automatic_fee);

        pimpl->config_loader.save();
        pimpl->config_loader.commit();
    }
}

coin config::get_automatic_fee() const
{
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

    coin result;

    if (pimpl->config_loader->automatic_fee)
        result = coin(*pimpl->config_loader->automatic_fee);

    return result;
}

void config::enable_action_log()
{
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

    pimpl->config_loader->enable_action_log = true;

    pimpl->config_loader.save();
    pimpl->config_loader.commit();
}

bool config::action_log() const
{
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

    if (pimpl->config_loader->enable_action_log)
        return *pimpl->config_loader->enable_action_log;

    return false;
}

void config::enable_inbox()
{
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

    pimpl->config_loader->enable_inbox = true;

    pimpl->config_loader.save();
    pimpl->config_loader.commit();
}

bool config::inbox() const
{
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

    if (pimpl->config_loader->enable_inbox)
        return *pimpl->config_loader->enable_inbox;

    return false;
}

void config::set_testnet()
{
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

    pimpl->config_loader->testnet = true;

    pimpl->config_loader.save();
    pimpl->config_loader.commit();
}

bool config::testnet() const
{
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

    if (pimpl->config_loader->testnet)
        return *pimpl->config_loader->testnet;

    return false;
}

void config::set_transfer_only()
{
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

    pimpl->config_loader->transfer_only = true;

    pimpl->config_loader.save();
    pimpl->config_loader.commit();
}

bool config::transfer_only() const
{
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

    if (pimpl->config_loader->transfer_only)
        return *pimpl->config_loader->transfer_only;

    return false;
}

void config::set_discovery_server()
{
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

    pimpl->config_loader->discovery_server = true;

    pimpl->config_loader.save();
    pimpl->config_loader.commit();
}

bool config::discovery_server() const
{
    auto locker = unique_lock<mutex>(pimpl->m_mutex);

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
