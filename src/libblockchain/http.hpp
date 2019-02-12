#pragma once

#include "message.hpp"

#include <belt.pp/parser.hpp>
#include <mesh.pp/http.hpp>

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
    auto it_fallback = iter_scan_begin;

    ssd.session_specal_handler = nullptr;
    ssd.autoreply.clear();

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

    auto code = meshpp::http::protocol(ssd,
                                       iter_scan_begin,
                                       iter_scan_end,
                                       it_fallback,
                                       1000,
                                       64 * 1024,
                                       10 * 1024 * 1024);

    meshpp::http::detail::scan_status* pss =
            dynamic_cast<meshpp::http::detail::scan_status*>(ssd.ptr_data.get());

    if (code == beltpp::e_three_state_result::error &&
        (nullptr == pss ||
         pss->status == meshpp::http::detail::scan_status::clean)
        )
    {
        if (pss)
        {
            ssd.ptr_data = beltpp::t_unique_nullptr<beltpp::detail::iscan_status>();
            ssd.parser_unrecognized_limit = 1024 * 1024;
        }

        return fallback_message_list_load(iter_scan_begin, iter_scan_end, ssd, putl);
    }
    else if (code == beltpp::e_three_state_result::error)
        return protocol_error();
    else if (code == beltpp::e_three_state_result::attempt)
    {
        iter_scan_begin = it_fallback;
        return ::beltpp::detail::pmsg_all(size_t(-1),
                                          ::beltpp::void_unique_nullptr(),
                                          nullptr);
    }
    else// if (code == beltpp::e_three_state_result::success)
    {
        if (pss->type == meshpp::http::detail::scan_status::get)
        {
            ssd.session_specal_handler = &file_response;
            ssd.autoreply.clear();
        }
        else if (pss->type == meshpp::http::detail::scan_status::post)
        {
            ssd.session_specal_handler = &meshpp::http::http_response;
            ssd.autoreply.clear();
        }

        if (pss->type == meshpp::http::detail::scan_status::get &&
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
        else if (pss->type == meshpp::http::detail::scan_status::post &&
                 pss->resource.path.size() == 1 &&
                 pss->resource.path.front() == "storage")
        {
            string cl = pss->resource.properties["Content-Length"];
            size_t pos;
            uint64_t content_length = beltpp::stoui64(cl, pos);

            {
                auto p = ::beltpp::new_void_unique_ptr<BlockchainMessage::StorageFile>();
                BlockchainMessage::StorageFile& ref = *reinterpret_cast<BlockchainMessage::StorageFile*>(p.get());
                ref.mime_type = pss->resource.properties["Content-Type"];

                ssd.ptr_data = beltpp::t_unique_nullptr<beltpp::detail::iscan_status>();

                for (size_t index = 0; index < content_length; ++index)
                {
                    assert(iter_scan_begin != iter_scan_end);
                    ++iter_scan_begin;
                }

                ref.data.assign(it_fallback, iter_scan_begin);

                return ::beltpp::detail::pmsg_all(BlockchainMessage::StorageFile::rtt,
                                                  std::move(p),
                                                  &BlockchainMessage::StorageFile::pvoid_saver);
            }
        }
        else if (pss->type == meshpp::http::detail::scan_status::post &&
                 pss->resource.path.size() == 1 &&
                 pss->resource.path.front() == "api")
        {
            string cl = pss->resource.properties["Content-Length"];
            size_t pos;
            uint64_t content_length = beltpp::stoui64(cl, pos);

            B_UNUSED(content_length);

            {
                ssd.ptr_data = beltpp::t_unique_nullptr<beltpp::detail::iscan_status>();
                ssd.parser_unrecognized_limit = 0;

                return fallback_message_list_load(iter_scan_begin, iter_scan_end, ssd, putl);
            }
        }
        else
            return protocol_error();
    }
}
}
}
