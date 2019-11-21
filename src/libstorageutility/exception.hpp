#pragma once

#include "global.hpp"

#include <exception>
#include <string>
#include <stdexcept>

namespace storage_utilitypp
{
class STORAGEUTILITYSHARED_EXPORT wrong_data_exception : public std::runtime_error
{
public:
    explicit wrong_data_exception(std::string const& _message);

    wrong_data_exception(wrong_data_exception const&) noexcept;
    wrong_data_exception& operator=(wrong_data_exception const&) noexcept;

    virtual ~wrong_data_exception() noexcept;

    std::string message;
};

class STORAGEUTILITYSHARED_EXPORT authority_exception : public std::runtime_error
{
public:
    explicit authority_exception(std::string const& authority_provided, std::string const& authority_required);

    authority_exception(authority_exception const&) noexcept;
    authority_exception& operator=(authority_exception const&) noexcept;

    virtual ~authority_exception() noexcept;

    std::string authority_provided;
    std::string authority_required;
};

class STORAGEUTILITYSHARED_EXPORT wrong_request_exception : public std::runtime_error
{
public:
    explicit wrong_request_exception(std::string const& _message);

    wrong_request_exception(wrong_request_exception const&) noexcept;
    wrong_request_exception& operator=(wrong_request_exception const&) noexcept;

    virtual ~wrong_request_exception() noexcept;

    std::string message;
};

class STORAGEUTILITYSHARED_EXPORT wrong_document_exception : public std::runtime_error
{
public:
    explicit wrong_document_exception(std::string const& _message);

    wrong_document_exception(wrong_document_exception const&) noexcept;
    wrong_document_exception& operator=(wrong_document_exception const&) noexcept;

    virtual ~wrong_document_exception() noexcept;

    std::string message;
};


class STORAGEUTILITYSHARED_EXPORT too_long_string_exception : public std::runtime_error
{
public:
    explicit too_long_string_exception(std::string const& used_string, size_t max_length);

    too_long_string_exception(too_long_string_exception const&) noexcept;
    too_long_string_exception& operator=(too_long_string_exception const&) noexcept;

    virtual ~too_long_string_exception() noexcept;

    std::string used_string;
    size_t max_length;
};

class STORAGEUTILITYSHARED_EXPORT uri_exception : public std::runtime_error
{
public:
    enum Type { missing, duplicate, invalid };
    explicit uri_exception(std::string const& uri, Type _uri_problem_type);

    uri_exception(uri_exception const&) noexcept;
    uri_exception& operator=(uri_exception const&) noexcept;

    virtual ~uri_exception() noexcept;

    std::string uri;
    Type uri_problem_type;
};
}// end of namespace publiqpp
