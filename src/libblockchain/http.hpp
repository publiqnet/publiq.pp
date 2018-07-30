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
    enum e_type {get, post};
    scan_status()
        : status(clean)
        , type(post)
        , http_header_scanning(0)
    {}
    e_status status;
    e_type type;
    size_t http_header_scanning;
    string resourse;
};

inline
string http_response(beltpp::detail::session_special_data& ssd,
                     beltpp::packet const& pc)
{
    string buffer = pc.to_string();

    ssd.session_specal_handler = nullptr;
    string str_result;
    str_result += "HTTP/1.1 200 OK\r\n";
    str_result += "Content-Type: application/json\r\n";
    str_result += "Content-Length: ";
    str_result += std::to_string(buffer.size());
    str_result += "\r\n\r\n";
    str_result += buffer;

    return str_result;
}

inline
string file_response(beltpp::detail::session_special_data& ssd,
                     beltpp::packet const& pc)
{
    ssd.session_specal_handler = nullptr;
    if (pc.type() == BlockchainMessage::StorageFile::rtt)
    {
        string str_result;
        BlockchainMessage::StorageFile const* pFile = nullptr;
        pc.get(pFile);

        str_result += "HTTP/1.1 200 OK\r\n";
        if (false == pFile->mime_type.empty())
            str_result += "Content-Type: " + pFile->mime_type + "\r\n";
        str_result += "Content-Length: ";
        str_result += std::to_string(pFile->data.length());
        str_result += "\r\n\r\n";
        str_result += pFile->data;

        return str_result;
    }
    else
    {
        string str_result;
        string message;
        if (pc.type() == BlockchainMessage::FileNotFound::rtt)
        {
            BlockchainMessage::FileNotFound const* pError = nullptr;
            pc.get(pError);
            message = "404 Not Found\r\n"
                    "requested file: " + pError->uri;
        }
        else
            message = "internal error";

        str_result += "HTTP/1.1 404 Not Found\r\n";
        str_result += "Content-Type: text/plain\r\n";
        str_result += "Content-Length: " + std::to_string(message.length()) + "\r\n\r\n";
        str_result += message;
        return str_result;
    }
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

    scan_status& ss = *reinterpret_cast<scan_status*>(ssd.ptr_data.get());
    size_t& http_header_scanning = ss.http_header_scanning;

    string value_post = "POST ", value_get = "GET ";
    if (scan_status::clean == ss.status)
    {
        auto iter_scan = check_begin(iter_scan_begin, iter_scan_end, value_post);
        if (iter_scan_begin == iter_scan)
            iter_scan = check_begin(iter_scan_begin, iter_scan_end, value_get);

        if (iter_scan_begin != iter_scan)
        {   //  even if "P" or "G" occured switch to http mode
            string temp(iter_scan_begin, iter_scan);
            ss.status = scan_status::http_request_progress;
        }
    }
    if (scan_status::http_request_progress == ss.status)
    {
        bool full = false;
        auto iter_scan1 = check_begin(iter_scan_begin, iter_scan_end, value_post);
        if (iter_scan_begin == iter_scan1)
            iter_scan1 = check_begin(iter_scan_begin, iter_scan_end, value_get);
        auto iter_scan2 = check_end(iter_scan_begin, iter_scan_end, " HTTP/1.1\r\n", full);

        string scanned_begin(iter_scan_begin, iter_scan1);

        if (value_post == scanned_begin ||
            value_get == scanned_begin)
        {
            string temp(iter_scan_begin, iter_scan2);

            if (full)
            {
                if (scanned_begin == value_get)
                    ss.type = scan_status::get;
                http_header_scanning += temp.length();
                iter_scan_begin = iter_scan2;
                ss.status = scan_status::http_properties_progress;
                string temp2(iter_scan1, iter_scan2);
                ss.resourse = temp2.substr(0, temp2.length() - 11);
            }
        }
    }
    while (scan_status::http_properties_progress == ss.status &&
           http_header_scanning < 1024 * 64)    //  don't support http header bigger than 64kb
    {
        bool full = false;
        auto iter_scan2 = check_end(iter_scan_begin, iter_scan_end, "\r\n", full);

        string temp(iter_scan_begin, iter_scan2);

        if (full)
        {
            http_header_scanning += temp.length();
            iter_scan_begin = iter_scan2;
            if (temp.length() == 2)
            {
                ss.status = scan_status::http_done;
                if (ss.type == scan_status::get)
                    ssd.session_specal_handler = &file_response;
                else
                    ssd.session_specal_handler = &http_response;
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
    {
        ss.status = scan_status::clean;
        if (ss.type == scan_status::get)
        {
            auto p = ::beltpp::new_void_unique_ptr<BlockchainMessage::StorageFileAddress>();
            BlockchainMessage::StorageFileAddress& ref = *reinterpret_cast<BlockchainMessage::StorageFileAddress*>(p.get());
            ref.uri = ss.resourse.substr(1);
            return ::beltpp::detail::pmsg_all(BlockchainMessage::StorageFileAddress::rtt,
                                              std::move(p),
                                              &BlockchainMessage::StorageFileAddress::pvoid_saver);
        }
        else
            return BlockchainMessage::message_list_load(iter_scan_begin, iter_scan_end, ssd, putl);
    }
}
}
}
