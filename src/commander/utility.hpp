#ifndef UTILITY_HPP
#define UTILITY_HPP

#include <string>
#include <publiq.pp/message.hpp>

using namespace BlockchainMessage;

class TransactionInfo
{

public:
    const TransactionInfo& get_transaction_info(TransactionLog const& transaction_log);
public:
    std::string from = std::string();
    std::string to = std::string();
    Coin amount = Coin();
    Coin fee = Coin();
};

#endif // UTILITY_HPP
