#include "coin.hpp"
#include "common.hpp"

#include <exception>

coin::coin()
    : whole(0)
    , fraction(0)
{}

coin::coin(coin const& other)
    : whole(other.whole)
    , fraction(other.fraction)
{}

coin::coin(uint64_t whole, uint64_t fraction)
    : whole(whole)
    , fraction(fraction)
{
    if (fraction >= fractions_in_whole)
        throw std::runtime_error("invalid coin amount: (" +
                                 std::to_string(whole) + ", " +
                                 std::to_string(fraction) + ")");
}

coin::coin(BlockchainMessage::Coin const& other)
    : coin(other.whole, other.fraction)
{}

uint64_t coin::to_uint64_t() const
{
    return fractions_in_whole * whole + fraction;
}

BlockchainMessage::Coin coin::to_Coin() const
{
    BlockchainMessage::Coin c;
    c.whole = whole;
    c.fraction = fraction;
    return c;
}

bool coin::empty() const
{
    return (whole == 0) && (fraction == 0);
}

coin& coin::operator = (coin const& other)
{
    whole = other.whole;
    fraction = other.fraction;

    return *this;
}

coin& coin::operator += (coin const& other)
{
    fraction += other.fraction;

    if (fraction >= fractions_in_whole)
    {
        ++whole;
        fraction -= fractions_in_whole;
    }

    whole += other.whole;

    return *this;
}

coin& coin::operator -= (coin const& other)
{
    if (    whole < other.whole ||
            (
                whole == other.whole &&
                fraction < other.fraction
            )
       )
        throw std::runtime_error("cannot have negative result for coin");

    if (fraction < other.fraction)
    {
        --whole;
        fraction += fractions_in_whole;
    }

    fraction -= other.fraction;

    whole -= other.whole;

    return *this;
}

coin& coin::operator *= (uint64_t const times)
{
    uint64_t add_to_whole = 0;

    uint64_t current_fraction = fraction * times;

    while (current_fraction >= fractions_in_whole)
    {
        current_fraction -= fractions_in_whole;
        add_to_whole++;
    }

    uint64_t current_whole = whole * times + add_to_whole;

    this->whole = current_whole;
    this->fraction = current_fraction;
    return *this;

}

coin& coin::operator /= (uint64_t const times)
{

    uint64_t current_fraction;
    uint64_t current_whole;

    if (times <= 0)
    {
       throw std::runtime_error("zero division");
    }

    current_whole = whole / times;
    if (whole % times > 0)
    {
       current_fraction = ( (whole % times) * fractions_in_whole + fraction ) / times;
    }
    else
    {
        current_fraction = fraction / times;
    }

    this->whole = current_whole;
    this->fraction = current_fraction;
    return *this;

}

bool coin::operator > (coin const& other) const
{
    if (whole > other.whole ||
            (
                whole == other.whole &&
                fraction > other.fraction
            )
       )
        return true;

    return false;
}
bool coin::operator < (coin const& other) const
{
    return other > *this;
}
bool coin::operator >= (coin const& other) const
{
    return !(other > *this);
}
bool coin::operator <= (coin const& other) const
{
    return !(*this > other);
}

bool coin::operator == (coin const& other) const
{
    return !(*this > other) && !(other > *this);
}
bool coin::operator != (coin const& other) const
{
    return (*this > other) || (other > *this);
}

coin operator + (coin first, coin const& second)
{
    first += second;
    return first;
}

coin operator - (coin first, coin const& second)
{
    first -= second;
    return first;
}

coin operator * (coin first,  uint64_t const times)
{
    first *= times;
    return first;
}

coin operator / (coin first,  uint64_t const times)
{
    first /= times;
    return first;
}
