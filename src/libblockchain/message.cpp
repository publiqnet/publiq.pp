#include "message.hpp"
#include "message.tmpl.hpp"

//  for now workaround optional support like this
//  later try to make it more or less as it should
namespace BlockchainMessage
{
namespace detail
{
template <typename T>
inline
bool analyze_json(optional<T>& value,
                  ::beltpp::json::expression_tree* pexp,
                  ::beltpp::message_loader_utility const& utl);
template <typename T>
inline
std::string saver(optional<T> const& value);

template <typename T>
inline
bool less(optional<T> const& first,
          optional<T> const& second)
{
    std::less<std::string> c;
    return c(saver(first), saver(second));
}
}
}

#include "message.gen.cpp.hpp"

namespace BlockchainMessage
{
namespace detail
{

template <typename T>
inline
bool analyze_json(optional<T>& value,
                  ::beltpp::json::expression_tree* pexp,
                  ::beltpp::message_loader_utility const& utl)
{
    value.reset();
    bool code = true;
    if (nullptr == pexp)
        code = false;
    else
    {
        T optional_value{};
        if (analyze_json(optional_value, pexp, utl))
            value = std::move(optional_value);
        else
        {
            code = false;
        }
    }

    return code;
}
template <typename T>
std::string saver(optional<T> const& value)
{
    if (value)
        return saver(*value);
    return std::string();
}
}
}
