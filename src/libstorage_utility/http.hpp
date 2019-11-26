#pragma once

#include "types.hpp"

#include <belt.pp/parser.hpp>
#include <belt.pp/http.hpp>

#include <mesh.pp/cryptoutility.hpp>

#include <string>
#include <vector>
#include <utility>
#include <unordered_map>
#include <chrono>

using std::string;
using std::vector;
using std::pair;
using std::unordered_map;

namespace storage_utility
{
namespace http
{

inline
string json_response(beltpp::detail::session_special_data& ssd,
                     beltpp::packet const& pc)
{
    return beltpp::http::http_response(ssd, pc.to_string());
}

inline
string base64_response(beltpp::detail::session_special_data& ssd,
                       beltpp::packet const& pc)
{
    assert(pc.type() == StorageUtilityMessage::SignedStorageOrder::rtt);
    StorageUtilityMessage::SignedStorageOrder const* pSSO;
    pc.get(pSSO);

    string result;
    result += "{";
    result += "\"channel_order\":\"" + meshpp::to_base64(pc.to_string(), false) + "\",";
    result += "\"storage_address\":\"" + pSSO->order.storage_address + "\"";
    result += "}";
    return beltpp::http::http_response(ssd, result);
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

    string posted;
    auto code = beltpp::http::protocol(ssd,
                                       iter_scan_begin,
                                       iter_scan_end,
                                       it_fallback,
                                       1000,
                                       64 * 1024,
                                       10 * 1024 * 1024,
                                       posted);

    beltpp::http::detail::scan_status* pss =
            dynamic_cast<beltpp::http::detail::scan_status*>(ssd.ptr_data.get());

    if (code == beltpp::e_three_state_result::error &&
        (nullptr == pss ||
         pss->status == beltpp::http::detail::scan_status::clean)
        )
    {
        if (pss)
        {
            ssd.parser_unrecognized_limit = pss->parser_unrecognized_limit_backup;
            ssd.ptr_data = beltpp::t_unique_nullptr<beltpp::detail::iscan_status>();
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
        ssd.session_specal_handler = &json_response;
        ssd.autoreply.clear();

        if (pss->type == beltpp::http::detail::scan_status::get &&
                 pss->resource.path.size() == 1 &&
                 pss->resource.path.front() == "storage_order")
        {
            ssd.session_specal_handler = &base64_response;
            ssd.autoreply.clear();

            auto p = ::beltpp::new_void_unique_ptr<StorageUtilityMessage::SignRequest>();
            StorageUtilityMessage::SignRequest& ref = *reinterpret_cast<StorageUtilityMessage::SignRequest*>(p.get());

            size_t pos;

            ref.private_key =  pss->resource.arguments["private_key"];
            ref.order.storage_address = pss->resource.arguments["storage_address"];
            ref.order.file_uri = pss->resource.arguments["file_uri"];
            ref.order.seconds = beltpp::stoui64(pss->resource.arguments["seconds"], pos);
            ref.order.time.tm = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

            ssd.ptr_data = beltpp::t_unique_nullptr<beltpp::detail::iscan_status>();
            return ::beltpp::detail::pmsg_all(StorageUtilityMessage::SignRequest::rtt,
                                              std::move(p),
                                              &StorageUtilityMessage::SignRequest::pvoid_saver);
        }
        else if (pss->type == beltpp::http::detail::scan_status::post &&
                pss->resource.path.size() == 1 &&
                pss->resource.path.front() == "api")
        {
            std::string::const_iterator iter_scan_begin_temp = posted.cbegin();
            std::string::const_iterator const iter_scan_end_temp = posted.cend();

            ssd.ptr_data = beltpp::t_unique_nullptr<beltpp::detail::iscan_status>();
            ssd.parser_unrecognized_limit = 0;

            return fallback_message_list_load(iter_scan_begin_temp, iter_scan_end_temp, ssd, putl);
        }
        else if (pss->type == beltpp::http::detail::scan_status::get &&
                 pss->resource.path.size() == 1 &&
                 pss->resource.path.front() == "protocol")
        {
            ssd.session_specal_handler = nullptr;

            ssd.autoreply = beltpp::http::http_response(ssd, StorageUtilityMessage::detail::storage_json_schema());

            ssd.ptr_data = beltpp::t_unique_nullptr<beltpp::detail::iscan_status>();

            return ::beltpp::detail::pmsg_all(size_t(-1),
                                              ::beltpp::void_unique_nullptr(),
                                              nullptr);
        }
        else
        {
            ssd.session_specal_handler = nullptr;

            string message("noo! \r\n");

            for (auto const& dir : pss->resource.path)
                message += "/" + dir;
            message += "\r\n";
            for (auto const& arg : pss->resource.arguments)
                message += (arg.first + ": " + arg.second + "\r\n");
            message += "\r\n";
            message += "\r\n";
            for (auto const& prop : pss->resource.properties)
                message += (prop.first + ": " + prop.second + "\r\n");
            message += "that's an error! \r\n";
            message += "here's the protocol, by the way \r\n";

            ssd.autoreply = beltpp::http::http_not_found(ssd,
                                                         message +
                                                         StorageUtilityMessage::detail::storage_json_schema());

            ssd.ptr_data = beltpp::t_unique_nullptr<beltpp::detail::iscan_status>();

            return ::beltpp::detail::pmsg_all(size_t(-1),
                                              ::beltpp::void_unique_nullptr(),
                                              nullptr);
        }
    }
}
}
}
