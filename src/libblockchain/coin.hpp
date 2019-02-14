#pragma once

#include "global.hpp"
#include "message.hpp"

#include <string>

namespace publiqpp
{
class BLOCKCHAINSHARED_EXPORT coin
{
public:
    coin();
    coin(coin const& other);
    coin(uint64_t whole, uint64_t fraction);
    coin(BlockchainMessage::Coin const& number);

    uint64_t to_uint64_t() const;
    BlockchainMessage::Coin to_Coin() const;
    std::string to_string() const;
    bool empty() const;

    coin& operator = (coin const& other);
    coin& operator += (coin const& other);
    coin& operator -= (coin const& other);

    coin& operator *= (uint64_t  times);
    coin& operator /= (uint64_t  times);
    coin& operator %= (uint64_t  times);

    bool operator > (coin const& other) const;
    bool operator < (coin const& other) const;
    bool operator >= (coin const& other) const;
    bool operator <= (coin const& other) const;

    bool operator == (coin const& other) const;
    bool operator != (coin const& other) const;

    static const uint64_t fractions_in_whole = 100000000;

private:
    uint64_t whole;
    uint64_t fraction;
};

BLOCKCHAINSHARED_EXPORT coin operator + (coin first, coin const& second);
BLOCKCHAINSHARED_EXPORT coin operator - (coin first, coin const& second);

BLOCKCHAINSHARED_EXPORT coin operator * (coin first, uint64_t  times);
BLOCKCHAINSHARED_EXPORT coin operator / (coin first, uint64_t  times);
BLOCKCHAINSHARED_EXPORT coin operator % (coin first, uint64_t  times);
}// end namespace publiqpp
