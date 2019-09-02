#pragma once

#include <string>
#include <publiq.pp/message.tmpl.hpp>

class TransactionInfo
{
public:
    TransactionInfo(BlockchainMessage::TransactionLog const& transaction_log);
public:
    std::string from;
    std::string to;
    std::string message;
    BlockchainMessage::Coin amount;
    BlockchainMessage::Coin fee;
};

