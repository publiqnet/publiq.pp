#include "coin.hpp"
#include "common.hpp"

#include <exception>
#include <stdexcept>

namespace publiqpp
{
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

uint64_t coin::to_uint64_t() const
{
    return fractions_in_whole * whole + fraction;
}

std::string coin::to_string() const
{
    return "(" + std::to_string(whole) + "," + std::to_string(fraction) + ")";
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

coin& coin::operator *= (uint64_t times)
{
    whole *= times;
    fraction *= times;
    whole += fraction / fractions_in_whole;
    fraction = fraction % fractions_in_whole;

    return *this;
}

coin& coin::operator /= (uint64_t  times)
{
    if (0 == times) throw std::runtime_error("zero division");

    fraction += whole % times * fractions_in_whole;
    whole /= times;
    fraction /= times;

    return *this;
}

coin& coin::operator %= (uint64_t  times)
{
     *this -= (*this / times) * times;
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

coin operator * (coin first, uint64_t  times)
{
    first *= times;
    return first;
}

coin operator / (coin first, uint64_t  times)
{
    first /= times;
    return first;
}

coin operator % (coin first, uint64_t  times)
{
    first %= times;
    return first;
}
}// end namespace publiqpp
