#pragma once

#include "message.hpp"

#include <string>
#include <vector>

using std::string;
using std::vector;

namespace publiqpp
{
namespace http
{
class scan_status
{
public:
    enum e_status {clean, http_request_progress, http_properties_progress, http_done};
    scan_status()
        : status(clean)
    {}
    e_status status;
};

inline
string save_header(beltpp::detail::session_special_data& ssd,
                   string const& message_stream)
{
    ssd.session_specal_handler = nullptr;
    string str_result;
    str_result += "HTTP/1.1 200 OK\r\n";
    str_result += "Content-Type: application/json\r\n";
    str_result += "Content-Length: ";
    str_result += std::to_string(message_stream.size());
    str_result += "\r\n\r\n";

    return str_result;
}

inline
beltpp::iterator_wrapper<char const>
check_begin(beltpp::iterator_wrapper<char const> const& iter_scan_begin,
            beltpp::iterator_wrapper<char const> const& iter_scan_end,
            string const& value) noexcept
{
    auto it_scan = iter_scan_begin;
    auto it_value = value.begin();

    while (true)
    {
        if (it_scan == iter_scan_end ||
            it_value == value.end())
            break;
        if (*it_value == *it_scan)
        {
            ++it_value;
            ++it_scan;
        }
        else
        {
            it_scan = iter_scan_begin;
            break;
        }
    }

    return it_scan;
}

inline
beltpp::iterator_wrapper<char const>
check_end(beltpp::iterator_wrapper<char const> const& iter_scan_begin,
          beltpp::iterator_wrapper<char const> const& iter_scan_end,
          string const& value,
          bool& full) noexcept
{
    full = false;
    auto it_scan_begin = iter_scan_begin;
    auto it_scan = it_scan_begin;
    auto it_value = value.begin();

    while (true)
    {
        if (it_value == value.end())
        {
            full = true;
            break;
        }
        if (it_value != value.end() &&
            it_scan == iter_scan_end)
        {
            it_scan = iter_scan_begin;
            break;
        }
        if (*it_value == *it_scan)
        {
            ++it_value;
            ++it_scan;
        }
        else
        {
            it_value = value.begin();
            ++it_scan_begin;
            it_scan = it_scan_begin;
        }
    }

    return it_scan;
}

inline
beltpp::detail::pmsg_all message_list_load(
        beltpp::iterator_wrapper<char const>& iter_scan_begin,
        beltpp::iterator_wrapper<char const> const& iter_scan_end,
        beltpp::detail::session_special_data& ssd,
        void* putl)
{
    if (nullptr == ssd.ptr_data)
        ssd.ptr_data = beltpp::new_void_unique_ptr<scan_status>();
    auto it_backup = iter_scan_begin;

    size_t http_header_scanning = 0;

    scan_status& ss = *reinterpret_cast<scan_status*>(ssd.ptr_data.get());
    if (scan_status::clean == ss.status ||
        scan_status::http_done == ss.status)
    {
        string value_check = "POST ";
        auto iter_scan = check_begin(iter_scan_begin, iter_scan_end, value_check);
        if (iter_scan_begin != iter_scan)
        {
            string temp(iter_scan_begin, iter_scan);
            //  even if "P" occured switch to http mode
            ss.status = scan_status::http_request_progress;
        }
    }
    if (scan_status::http_request_progress == ss.status)
    {
        string value_check = "POST ";
        auto iter_scan1 = check_begin(iter_scan_begin, iter_scan_end, value_check);
        if (value_check == string(iter_scan_begin, iter_scan1))
        {
            bool full = false;
            auto iter_scan2 = check_end(iter_scan_begin, iter_scan_end, "\r\n", full);

            string temp(iter_scan_begin, iter_scan2);
            http_header_scanning += temp.length();

            if (full)
            {
                iter_scan_begin = iter_scan2;
                ss.status = scan_status::http_properties_progress;
            }
        }
    }
    while (scan_status::http_properties_progress == ss.status &&
           http_header_scanning < 1024 * 64)    //  don't support http header bigger than 64kb
    {
        bool full = false;
        auto iter_scan2 = check_end(iter_scan_begin, iter_scan_end, "\r\n", full);

        string temp(iter_scan_begin, iter_scan2);
        http_header_scanning += temp.length();

        if (full)
        {
            iter_scan_begin = iter_scan2;
            if (temp.length() == 2)
            {
                ss.status = scan_status::http_done;
                ssd.session_specal_handler = &save_header;
            }
        }
        else
            break;
    }

    if (http_header_scanning >= 64 * 1024)
    {
        ss.status = scan_status::clean;
        iter_scan_begin = iter_scan_end;
        return ::beltpp::detail::pmsg_all(size_t(-2),
                                          ::beltpp::void_unique_ptr(nullptr, [](void*){}),
                                          nullptr);
    }
    else if (scan_status::http_properties_progress == ss.status ||
             scan_status::http_request_progress == ss.status)
    {
        //  revert the cursor, so everything will be rescanned
        //  once there is more data to scan
        //  in future may implement persistent state, so rescan will not
        //  be needed
        ss.status = scan_status::clean;
        iter_scan_begin = it_backup;
        return ::beltpp::detail::pmsg_all(size_t(-1),
                                          ::beltpp::void_unique_ptr(nullptr, [](void*){}),
                                          nullptr);
    }
    else
        return BlockchainMessage::message_list_load(iter_scan_begin, iter_scan_end, ssd, putl);
}
}
}
