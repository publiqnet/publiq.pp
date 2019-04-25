#include "exception.hpp"
#include "common.hpp"

using std::string;

namespace publiqpp
{
wrong_data_exception::wrong_data_exception(std::string const& _message)
    : runtime_error("wrong_data_exception - " + _message)
    , message(_message)
{}
wrong_data_exception::wrong_data_exception(wrong_data_exception const& other) noexcept
    : runtime_error(other)
    , message(other.message)
{}
wrong_data_exception& wrong_data_exception::operator=(wrong_data_exception const& other) noexcept
{
    dynamic_cast<runtime_error*>(this)->operator =(other);
    message = other.message;
    return *this;
}
wrong_data_exception::~wrong_data_exception() noexcept
{}

authority_exception::authority_exception(std::string const& _authority_provided, std::string const& _authority_required)
    : runtime_error("Invalid authority! authority_provided: " + _authority_provided + "  " + " authority_required: " + _authority_required)
    , authority_provided(_authority_provided)
    , authority_required(_authority_required)
{}
authority_exception::authority_exception(authority_exception const& other) noexcept
    : runtime_error(other)
    , authority_provided(other.authority_provided)
    , authority_required(other.authority_required)
{}
authority_exception& authority_exception::operator=(authority_exception const& other) noexcept
{
    dynamic_cast<runtime_error*>(this)->operator =(other);
    authority_provided = other.authority_provided;
    authority_required = other.authority_required;
    return *this;
}
authority_exception::~authority_exception() noexcept
{}

wrong_request_exception::wrong_request_exception(std::string const& _message)
    : runtime_error("unexpected request - message: " + _message)
    , message(_message)
{}
wrong_request_exception::wrong_request_exception(wrong_request_exception const& other) noexcept
    : runtime_error(other)
    , message(other.message)
{}
wrong_request_exception& wrong_request_exception::operator=(wrong_request_exception const& other) noexcept
{
    dynamic_cast<runtime_error*>(this)->operator =(other);
    message = other.message;
    return *this;
}
wrong_request_exception::~wrong_request_exception() noexcept
{}

wrong_document_exception::wrong_document_exception(std::string const& _message)
    : runtime_error("wrong_document_exception - " + _message)
    , message(_message)
{}
wrong_document_exception::wrong_document_exception(wrong_document_exception const& other) noexcept
    : runtime_error(other)
    , message(other.message)
{}
wrong_document_exception& wrong_document_exception::operator=(wrong_document_exception const& other) noexcept
{
    dynamic_cast<runtime_error*>(this)->operator =(other);
    message = other.message;
    return *this;
}
wrong_document_exception::~wrong_document_exception() noexcept
{}

not_enough_balance_exception::not_enough_balance_exception(coin const& balance, coin const& spending)
    : runtime_error("trying to spend more than available. "
                    "balance is " + balance.to_string() + ", "
                    "while trying to spend (" + spending.to_string() + ")")
    , balance(balance)
    , spending(spending)
{}
not_enough_balance_exception::not_enough_balance_exception(not_enough_balance_exception const& other) noexcept
    : runtime_error(other)
    , balance(other.balance)
    , spending(other.spending)
{}
not_enough_balance_exception& not_enough_balance_exception::operator=(not_enough_balance_exception const& other) noexcept
{
    dynamic_cast<runtime_error*>(this)->operator =(other);
    balance = other.balance;
    spending = other.spending;
    return *this;
}
not_enough_balance_exception::~not_enough_balance_exception() noexcept
{}

too_long_string_exception::too_long_string_exception(string const& used_string, size_t max_length)
    : runtime_error(used_string + " - is longer than allowed. " +
                    std::to_string(used_string.size()) + " vs " +
                    std::to_string(max_length))
    , used_string(used_string)
    , max_length(max_length)
{}
too_long_string_exception::too_long_string_exception(too_long_string_exception const& other) noexcept
    : runtime_error(other)
    , used_string(other.used_string)
    , max_length(other.max_length)
{}
too_long_string_exception& too_long_string_exception::operator=(too_long_string_exception const& other) noexcept
{
    dynamic_cast<runtime_error*>(this)->operator =(other);
    used_string = other.used_string;
    max_length = other.max_length;
    return *this;
}
too_long_string_exception::~too_long_string_exception() noexcept
{}

string uri_exception_prefix(uri_exception::Type uri_problem_type)
{
    switch (uri_problem_type)
    {
    case uri_exception::duplicate:
        return "duplicate uri ";
    case uri_exception::invalid:
        return "invalid uri ";
    case uri_exception::missing:
        return "missing uri ";
    }
    //  msvc thinks this is an execution path that needs to be covered
    assert(false);
    throw std::logic_error("why do I have to use 'default:' even when all cases are handled?");
}
uri_exception::uri_exception(string const& _uri, uri_exception::Type _uri_problem_type)
    : runtime_error(uri_exception_prefix(_uri_problem_type) + _uri)
    , uri(_uri)
    , uri_problem_type(_uri_problem_type)
{}
uri_exception::uri_exception(uri_exception const& other) noexcept
    : runtime_error(other)
    , uri(other.uri)
    , uri_problem_type(other.uri_problem_type)
{}
uri_exception& uri_exception::operator=(uri_exception const& other) noexcept
{
    dynamic_cast<runtime_error*>(this)->operator =(other);
    uri = other.uri;
    uri_problem_type = other.uri_problem_type;
    return *this;
}
uri_exception::~uri_exception() noexcept
{}
}// end of namespace publiqpp
