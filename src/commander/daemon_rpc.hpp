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
    using LogIndexLoader = meshpp::map_loader<CommanderMessage::NumberPair>;
    daemon_rpc();

    void open(beltpp::ip_address const& connect_to_address);
    void close();

    beltpp::packet process_storage_update_request(CommanderMessage::StorageUpdateRequest const& update,
                                                  rpc& rpc_server);
    beltpp::packet send(CommanderMessage::Send const& send,
                        rpc& rpc_server);
    beltpp::packet wait_response(std::string const& transaction_hash);
    void sync(rpc& rpc_server,
              std::unordered_set<std::string> const& set_accounts,
              bool const new_import);

    static
    TransactionLogLoader get_transaction_log(std::string const& address);
    static
    RewardLogLoader get_reward_log(std::string const& address);
    static
    LogIndexLoader get_transaction_log_index(std::string const& address);
    static
    LogIndexLoader get_reward_log_index(std::string const& address);

    beltpp::event_handler eh;
    beltpp::socket socket;
    beltpp::isocket::peer_id peerid;
    meshpp::file_loader<CommanderMessage::NumberValue, &CommanderMessage::NumberValue::from_string, &CommanderMessage::NumberValue::to_string> log_index;
};
