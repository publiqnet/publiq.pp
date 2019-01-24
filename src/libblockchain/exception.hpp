#pragma once

#include <exception>
#include <string>

class wrong_data_exception : public std::runtime_error
{
public:
    explicit wrong_data_exception(std::string const& _message);

    wrong_data_exception(wrong_data_exception const&) noexcept;
    wrong_data_exception& operator=(wrong_data_exception const&) noexcept;

    virtual ~wrong_data_exception() noexcept;

    std::string message;
};

class authority_exception : public std::runtime_error
{
public:
    explicit authority_exception(std::string const& authority_provided, std::string const& authority_required);

    authority_exception(authority_exception const&) noexcept;
    authority_exception& operator=(authority_exception const&) noexcept;

    virtual ~authority_exception() noexcept;

    std::string authority_provided;
    std::string authority_required;
};

class wrong_request_exception : public std::runtime_error
{
public:
    explicit wrong_request_exception(std::string const& _message);

    wrong_request_exception(wrong_request_exception const&) noexcept;
    wrong_request_exception& operator=(wrong_request_exception const&) noexcept;

    virtual ~wrong_request_exception() noexcept;

    std::string message;
};
