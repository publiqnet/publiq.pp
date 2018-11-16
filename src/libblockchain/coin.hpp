#pragma once

#include "global.hpp"
#include "message.hpp"

#include <string>

class coin
{
public:
    coin();
    coin(coin const& other);
    coin(uint64_t whole, uint64_t fraction);
    coin(BlockchainMessage::Coin const& number);

    uint64_t to_uint64_t() const;
    BlockchainMessage::Coin to_Coin() const;
    bool empty() const;

    coin& operator = (coin const& other);
    coin& operator += (coin const& other);
    coin& operator -= (coin const& other);

    coin& operator *= (uint64_t const times);
    coin& operator /= (uint64_t const times);

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

coin operator + (coin first, coin const& second);
coin operator - (coin first, coin const& second);

coin operator * (coin first, coin const& second);
coin operator / (coin first, coin const& second);
