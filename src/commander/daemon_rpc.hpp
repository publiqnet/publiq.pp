#pragma once

#include "commander_message.hpp"

#include <belt.pp/socket.hpp>

#include <mesh.pp/fileutility.hpp>

#include <publiq.pp/message.hpp>
#include <publiq.pp/message.tmpl.hpp>

#include <utility>
#include <unordered_set>
#include <string>
#include <map>
class rpc;

class daemon_rpc
{
public:
    using TransactionLogLoader = meshpp::vector_loader<BlockchainMessage::TransactionLog>;
    using RewardLogLoader = meshpp::vector_loader<BlockchainMessage::RewardLog>;
    using StorageUpdateLogLoader = meshpp::vector_loader<BlockchainMessage::StorageUpdate>;
    using ContentUnitLogLoader = meshpp::vector_loader<BlockchainMessage::ContentUnit>;
    using ContentLogLoader = meshpp::vector_loader<BlockchainMessage::Content>;
    using LogIndexLoader = meshpp::map_loader<CommanderMessage::NumberPair>;
    daemon_rpc();

    void open(beltpp::ip_address const& connect_to_address);
    void close();

    beltpp::packet send(CommanderMessage::Send const& send,
                        rpc& rpc_server);
    void sync(rpc& rpc_server,
              std::unordered_set<std::string> const& set_accounts,
              bool const new_import);

    static
    TransactionLogLoader get_transaction_log(std::string const& address);
    static
    RewardLogLoader get_reward_log(std::string const& address);
    static
    StorageUpdateLogLoader get_storage_update_log(std::string const& address);
    static
    ContentUnitLogLoader get_content_unit_log();
    static
    ContentLogLoader get_content_log();
    static
    LogIndexLoader get_transaction_log_index(std::string const& address);
    static
    LogIndexLoader get_reward_log_index(std::string const& address);
    static
    LogIndexLoader get_storage_update_log_index(std::string const& address);
    static
    LogIndexLoader get_content_unit_log_index();
    static
    LogIndexLoader get_content_log_index();

    beltpp::event_handler eh;
    beltpp::socket socket;
    beltpp::isocket::peer_id peerid;
    meshpp::file_loader<CommanderMessage::NumberValue, &CommanderMessage::NumberValue::from_string, &CommanderMessage::NumberValue::to_string> log_index;
};
