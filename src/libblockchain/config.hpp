#pragma once

#include "global.hpp"
#include "message.hpp"
#include "coin.hpp"

#include <belt.pp/socket.hpp>

#include <mesh.pp/cryptoutility.hpp>

#include <memory>
#include <string>

namespace publiqpp
{

namespace detail
{
class config_internal;
}

class BLOCKCHAINSHARED_EXPORT config
{
public:
    config();
    config(config&&) noexcept;
    ~config();

    config& operator = (config&& other) noexcept;

    void set_data_directory(std::string const& str_data_directory);
    //std::string get_data_directory() const;
    void set_aes_key(std::string const& key);

    void set_p2p_bind_to_address(beltpp::ip_address const& address);
    beltpp::ip_address get_p2p_bind_to_address() const;

    void set_p2p_connect_to_addresses(std::vector<beltpp::ip_address> const& addresses);
    std::vector<beltpp::ip_address> get_p2p_connect_to_addresses() const;

    void set_rpc_bind_to_address(beltpp::ip_address const& address);
    beltpp::ip_address get_rpc_bind_to_address() const;

    void set_slave_bind_to_address(beltpp::ip_address const& address);
    beltpp::ip_address get_slave_bind_to_address() const;

    void set_public_address(beltpp::ip_address const& address);
    beltpp::ip_address get_public_address() const;

    void set_public_ssl_address(beltpp::ip_address const& address);
    beltpp::ip_address get_public_ssl_address() const;

    void set_manager_address(std::string const& manager_address);
    std::string get_manager_address() const;

    void set_key(meshpp::private_key const& pk);
    meshpp::private_key get_key() const;
    bool is_key_set() const;

    void add_secondary_key(meshpp::private_key const& pk);
    void remove_secondary_key(meshpp::private_key const& pk);
    std::vector<meshpp::private_key> keys() const;

    void set_node_type(std::string const& node_type);
    BlockchainMessage::NodeType get_node_type() const;

    void set_automatic_fee(size_t fractions);
    coin get_automatic_fee() const;

    void enable_action_log();
    bool action_log() const;

    void enable_inbox();
    bool inbox() const;

    void set_testnet();
    bool testnet() const;

    void set_transfer_only();
    bool transfer_only() const;

    void set_discovery_server();
    bool discovery_server() const;

    std::string check_for_error() const;

    std::unique_ptr<detail::config_internal> pimpl;
};
}
