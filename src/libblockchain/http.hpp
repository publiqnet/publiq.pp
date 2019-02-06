#pragma once

#include "message.hpp"

#include <string>
#include <vector>
#include <utility>
#include <unordered_map>

using std::string;
using std::vector;
using std::pair;
using std::unordered_map;

namespace publiqpp
{
namespace http
{

class request
{
    static
    vector<string> split(string const& value,
                         string const& separator,
                         bool skip_empty,
                         size_t limit)
    {
        vector<string> parts;

        size_t find_index = 0;

        while (find_index < value.size() &&
               parts.size() < limit)
        {
            size_t next_index = value.find(separator, find_index);
            if (next_index == string::npos)
                next_index = value.size();

            string part = value.substr(find_index, next_index - find_index);

            if (false == part.empty() || false == skip_empty)
                parts.push_back(part);

            find_index = next_index + separator.size();
        }

        return parts;
    }

    static size_t from_hex(string const& value)
    {
        size_t result = 0;

        for (auto const& item : value)
        {
            if (item >= '0' && item <= '9')
            {
                result *= 16;
                result += size_t(item - '0');
            }
            else if (item >= 'a' && item <= 'f')
            {
                result *= 16;
                result += size_t(item - 'a' + 10);
            }
            else if (item >= 'A' && item <= 'F')
            {
                result *= 16;
                result += size_t(item - 'A' + 10);
            }
            else
                return size_t(-1);
        }

        return result;
    }

    static
    string percent_decode(string const& value)
    {
        string result;

        size_t find_index = 0;

        while (find_index < value.size())
        {
            size_t next_index = value.find("%", find_index);
            if (next_index == string::npos)
                next_index = value.size();

            if (next_index + 2 >= value.size() ||
                size_t(-1) == from_hex(value.substr(next_index + 1, 2)))
                result += value.substr(find_index, value.size() - find_index);
            else
            {
                result += value.substr(find_index, next_index - find_index);
                result += char(from_hex(value.substr(next_index + 1, 2)));
            }

            find_index = next_index + 3;
        }

        return result;
    }
public:
    vector<string> path;
    unordered_map<string, string> arguments;
    unordered_map<string, string> properties;

    bool set(string const& url)
    {
        bool code = true;

        vector<string> parts = split(url, "?", true, 2);
        string part_path, part_query, part_hash;

        if (parts.empty() ||
            parts.size() > 2)
            code = false;
        if (parts.size() == 1)
            part_path = parts.front();
        else
        {
            part_path = parts.front();

            string query_and_hash = parts.back();

            parts = split(query_and_hash, "#", true, 2);

            if (parts.empty() ||
                parts.size() > 2)
                code = false;
            else if (parts.size() == 1)
                part_query = parts.front();
            else
            {
                part_query = parts.front();
                part_hash = parts.back();
            }
        }

        path = split(part_path, "/", true, size_t(-1));

        for (auto& path_item : path)
            path_item = percent_decode(path_item);

        parts = split(part_query, "&", true, size_t(-1));
        for (auto const& part_item : parts)
        {
            vector<string> subparts = split(part_item, "=", true, 2);
            if (subparts.empty() ||
                subparts.size() > 2)
                code = false;
            else if (subparts.size() == 1)
                arguments[percent_decode(subparts.front())] = string();
            else
                arguments[percent_decode(subparts.front())] =
                        percent_decode(subparts.back());
        }

        return code;
    }

    bool add_property(string const& value)
    {
        vector<string> parts = split(value, ": ", false, 2);

        if (parts.empty() ||
            parts.size() > 2)
            return false;
        else if (parts.size() == 1)
            properties[parts.front()] = string();
        else
            properties[parts.front()] = parts.back();

        return true;
    }
};


class scan_status : public beltpp::detail::iscan_status
{
public:
    enum e_status {clean, http_request_progress, http_properties_progress, http_done};
    enum e_type {get, post, options};
    scan_status()
        : status(clean)
        , type(post)
        , http_header_scanning(0)
    {}
    ~scan_status() override
    {}
    e_status status;
    e_type type;
    size_t http_header_scanning;
    request resource;
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
    str_result += "Access-Control-Allow-Origin: *\r\n";
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
std::string::const_iterator
check_begin(std::string::const_iterator const& iter_scan_begin,
            std::string::const_iterator const& iter_scan_end,
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
std::pair<std::string::const_iterator, std::string::const_iterator>
check_end(std::string::const_iterator const& iter_scan_begin,
          std::string::const_iterator const& iter_scan_end,
          string const& value) noexcept
{
    auto it_scan_begin = iter_scan_begin;
    auto it_scan = it_scan_begin;
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
            it_value = value.begin();
            ++it_scan_begin;
            it_scan = it_scan_begin;
        }
    }

    return std::make_pair(it_scan_begin, it_scan);
}

template <beltpp::detail::pmsg_all (*fallback_message_list_load)(
        std::string::const_iterator&,
        std::string::const_iterator const&,
        beltpp::detail::session_special_data&,
        void*)>
beltpp::detail::pmsg_all message_list_load(
        std::string::const_iterator& iter_scan_begin,
        std::string::const_iterator const& iter_scan_end,
        beltpp::detail::session_special_data& ssd,
        void* putl)
{
    if (nullptr == ssd.ptr_data)
        ssd.ptr_data = beltpp::new_dc_unique_ptr<beltpp::detail::iscan_status, scan_status>();

    auto it_fallback = iter_scan_begin;

    ssd.session_specal_handler = nullptr;
    ssd.autoreply.clear();

    scan_status* pss = dynamic_cast<scan_status*>(ssd.ptr_data.get());

    auto protocol_error = [&iter_scan_begin, &iter_scan_end, &ssd]()
    {
        ssd.ptr_data = beltpp::t_unique_nullptr<beltpp::detail::iscan_status>();
        ssd.session_specal_handler = nullptr;
        ssd.autoreply.clear();
        iter_scan_begin = iter_scan_end;
        return ::beltpp::detail::pmsg_all(size_t(-2),
                                          ::beltpp::void_unique_nullptr(),
                                          nullptr);
    };

    string line(iter_scan_begin, iter_scan_end);
    size_t line_length = line.length();

    bool enough_length = (line_length > 1000);

    string value_post = "POST ", value_get = "GET ", value_options = "OPTIONS ";

    if (pss &&
        scan_status::clean == pss->status)
    {

        auto iter_scan = check_begin(iter_scan_begin, iter_scan_end, value_post);
        if (iter_scan_begin == iter_scan)
            iter_scan = check_begin(iter_scan_begin, iter_scan_end, value_get);
        if (iter_scan_begin == iter_scan)
            iter_scan = check_begin(iter_scan_begin, iter_scan_end, value_options);

        if (iter_scan_begin != iter_scan)
        {   //  even if "P", "G" or "O" occured switch to http mode
            string temp(iter_scan_begin, iter_scan);
            pss->status = scan_status::http_request_progress;
        }
    }

    if (pss &&
        scan_status::http_request_progress == pss->status)
    {
        string const value_ending = " HTTP/1.1\r\n";
        auto iter_scan_check_begin = check_begin(iter_scan_begin, iter_scan_end, value_post);
        if (iter_scan_begin == iter_scan_check_begin)
            iter_scan_check_begin = check_begin(iter_scan_begin, iter_scan_end, value_get);
        if (iter_scan_begin == iter_scan_check_begin)
            iter_scan_check_begin = check_begin(iter_scan_begin, iter_scan_end, value_options);

        auto iter_scan_check_end = check_end(iter_scan_begin, iter_scan_end, value_ending);

        string scanned_begin(iter_scan_begin, iter_scan_check_begin);
        string scanned_ending(iter_scan_check_end.first, iter_scan_check_end.second);

        if (scanned_begin.empty() ||
            (scanned_ending.empty() && enough_length))
            return protocol_error();
        else if ((scanned_begin != value_post &&
                  scanned_begin != value_get &&
                  scanned_begin != value_options) ||
                 scanned_ending != value_ending)
        {
            //  that's ok, wait for more data
        }
        else
        {
            string scanned_line(iter_scan_begin, iter_scan_check_end.second);

            if (scanned_begin == value_get)
                pss->type = scan_status::get;
            else if (scanned_begin == value_post)
                pss->type = scan_status::post;
            else// if (scanned_begin == value_options)
                pss->type = scan_status::options;

            pss->http_header_scanning += scanned_line.length();
            iter_scan_begin = iter_scan_check_end.second;
            it_fallback = iter_scan_begin;

            pss->status = scan_status::http_properties_progress;

            if (false == pss->resource.set(string(iter_scan_check_begin,
                                                  iter_scan_check_end.first)))
                    return protocol_error();
        }
    }
    while (pss &&
           scan_status::http_properties_progress == pss->status &&
           pss->http_header_scanning < 1024 * 64)    //  don't support http header bigger than 64kb
    {
        string const value_ending = "\r\n";
        auto iter_scan_check_end = check_end(iter_scan_begin, iter_scan_end, value_ending);

        string scanned_begin(iter_scan_begin, iter_scan_check_end.first);
        string scanned_ending(iter_scan_check_end.first, iter_scan_check_end.second);

        if (scanned_ending.empty() && enough_length)
            return protocol_error();
        else if (scanned_ending != value_ending)
        {
            //  that's ok, wait for more data
            break;
        }
        else
        {
            string scanned_line(iter_scan_begin, iter_scan_check_end.second);
            pss->http_header_scanning += scanned_line.length();

            iter_scan_begin = iter_scan_check_end.second;
            it_fallback = iter_scan_begin;

            if (scanned_begin.empty())
                pss->status = scan_status::http_done;
            else if(false == pss->resource.add_property(scanned_begin))
                return protocol_error();
        }
    }

    if (pss &&
        pss->http_header_scanning >= 64 * 1024)
        return protocol_error();
    else if (pss &&
             (scan_status::http_properties_progress == pss->status ||
              scan_status::http_request_progress == pss->status))
    {
        //  revert the cursor, to last parsed position
        iter_scan_begin = it_fallback;
        return ::beltpp::detail::pmsg_all(size_t(-1),
                                          ::beltpp::void_unique_nullptr(),
                                          nullptr);
    }
    else if (pss &&
             pss->status == scan_status::http_done)
    {
        //  revert the cursor, to after http header
        iter_scan_begin = it_fallback;
        line.assign(iter_scan_begin, iter_scan_end);
        line_length = line.length();

        if (pss->type == scan_status::get)
        {
            ssd.session_specal_handler = &file_response;
            ssd.autoreply.clear();
        }
        else if (pss->type == scan_status::post)
        {
            ssd.session_specal_handler = &http_response;
            ssd.autoreply.clear();
        }

        if (pss->type == scan_status::get &&
            pss->resource.path.size() == 1 &&
            pss->resource.path.front() == "storage")
        {
            auto p = ::beltpp::new_void_unique_ptr<BlockchainMessage::GetStorageFile>();
            BlockchainMessage::GetStorageFile& ref = *reinterpret_cast<BlockchainMessage::GetStorageFile*>(p.get());
            ref.uri = pss->resource.arguments["file"];
            ssd.ptr_data = beltpp::t_unique_nullptr<beltpp::detail::iscan_status>();
            return ::beltpp::detail::pmsg_all(BlockchainMessage::GetStorageFile::rtt,
                                              std::move(p),
                                              &BlockchainMessage::GetStorageFile::pvoid_saver);
        }
        else if (pss->type == scan_status::post &&
                 pss->resource.path.size() == 1 &&
                 pss->resource.path.front() == "storage")
        {
            string cl = pss->resource.properties["Content-Length"];
            size_t pos;
            uint64_t content_length = beltpp::stoui64(cl, pos);

            if (cl.empty() ||
                pos != cl.length() ||
                content_length > 10 * 1024 * 1024)
                return protocol_error();
            else if (line.length() < content_length)
                return ::beltpp::detail::pmsg_all(size_t(-1),
                                                  ::beltpp::void_unique_nullptr(),
                                                  nullptr);
            else
            {
                auto p = ::beltpp::new_void_unique_ptr<BlockchainMessage::StorageFile>();
                BlockchainMessage::StorageFile& ref = *reinterpret_cast<BlockchainMessage::StorageFile*>(p.get());
                ref.mime_type = pss->resource.properties["Content-Type"];

                ssd.ptr_data = beltpp::t_unique_nullptr<beltpp::detail::iscan_status>();

                for (size_t index = 0; index < content_length; ++index)
                    ++iter_scan_begin;

                ref.data.assign(it_fallback, iter_scan_begin);

                return ::beltpp::detail::pmsg_all(BlockchainMessage::StorageFile::rtt,
                                                  std::move(p),
                                                  &BlockchainMessage::StorageFile::pvoid_saver);
            }
        }
        else if (pss->type == scan_status::post &&
                 pss->resource.path.size() == 1 &&
                 pss->resource.path.front() == "api")
        {
            string cl = pss->resource.properties["Content-Length"];
            size_t pos;
            uint64_t content_length = beltpp::stoui64(cl, pos);

            if (cl.empty() ||
                pos != cl.length() ||
                content_length > 10 * 1024 * 1024)
                return protocol_error();
            else if (line.length() < content_length)
                return ::beltpp::detail::pmsg_all(size_t(-1),
                                                  ::beltpp::void_unique_nullptr(),
                                                  nullptr);
            else
            {
                ssd.ptr_data = beltpp::t_unique_nullptr<beltpp::detail::iscan_status>();
                ssd.parser_unrecognized_limit = 0;

                return fallback_message_list_load(iter_scan_begin, iter_scan_end, ssd, putl);
                //return BlockchainMessage::message_list_load(iter_scan_begin, iter_scan_end, ssd, putl);
            }
        }
        else if (pss->type == scan_status::options)
        {
            ssd.session_specal_handler = nullptr;

            ssd.autoreply += "HTTP/1.1 200 OK\r\n";
            ssd.autoreply += "Access-Control-Allow-Origin: *\r\n";
            ssd.autoreply += "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n";
            ssd.autoreply += "Access-Control-Allow-Headers: X-CUSTOM-HEADER, Content-Type\r\n";
            ssd.autoreply += "Access-Control-Max-Age: 86400\r\n";
            //ssd.autoreply += "Vary: Accept-Encoding, Origin\r\n";
            ssd.autoreply += "Content-Length: 0\r\n";
            ssd.autoreply += "\r\n";

            ssd.ptr_data = beltpp::t_unique_nullptr<beltpp::detail::iscan_status>();

            return ::beltpp::detail::pmsg_all(size_t(-1),
                                              ::beltpp::void_unique_nullptr(),
                                              nullptr);
        }
        else
            return protocol_error();
    }
    else
    {
        if (pss)
        {
            ssd.ptr_data = beltpp::t_unique_nullptr<beltpp::detail::iscan_status>();
            ssd.parser_unrecognized_limit = 1024 * 1024;
        }

        return fallback_message_list_load(iter_scan_begin, iter_scan_end, ssd, putl);
        //return BlockchainMessage::message_list_load(iter_scan_begin, iter_scan_end, ssd, putl);
    }
}
}
}
