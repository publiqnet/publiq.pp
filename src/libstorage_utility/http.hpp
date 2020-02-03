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

#define SIGN_SECONDS 3600

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
string storage_order_response(beltpp::detail::session_special_data& ssd,
                              beltpp::packet const& pc)
{
    string result;
    if (pc.type() == StorageUtilityMessage::SignedStorageOrder::rtt)
    {
        StorageUtilityMessage::SignedStorageOrder const* pSSO;
        pc.get(pSSO);

        result += "{";
        result += "\"code\":true,";
        result += "\"storage_order\":\"" + meshpp::to_base64(pc.to_string(), false) + "\",";
        result += "\"storage_address\":\"" + pSSO->order.storage_address + "\"";
        result += "}";
    }
    else
    {
        string message = "\"unknown\"";
        if (pc.type() == StorageUtilityMessage::RemoteError::rtt)
        {
            StorageUtilityMessage::RemoteError const* pError;
            pc.get(pError);
            message = beltpp::json::value_string::encode(pError->message);
        }

        result += "{";
        result += "\"code\":false,";
        result += "\"reason\":" + message;
        result += "}";
    }
    return beltpp::http::http_response(ssd, result);
}

inline
string storage_verify_response(beltpp::detail::session_special_data& ssd,
                               beltpp::packet const& pc)
{
    string result;
    if (pc.type() == StorageUtilityMessage::VerificationResponse::rtt)
    {
        result += "{";
        result += "\"code\":true";
        result += "}";
    }
    else
    {
        string message = "\"unknown\"";
        if (pc.type() == StorageUtilityMessage::RemoteError::rtt)
        {
            StorageUtilityMessage::RemoteError const* pError;
            pc.get(pError);
            message = beltpp::json::value_string::encode(pError->message);
        }

        result += "{";
        result += "\"code\":false,";
        result += "\"reason\":" + message ;
        result += "}";
    }
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
        ssd.session_specal_handler = nullptr;
        ssd.autoreply.clear();
        iter_scan_begin = iter_scan_end;
        return ::beltpp::detail::pmsg_all(size_t(-2),
                                          ::beltpp::void_unique_nullptr(),
                                          nullptr);
    };

    string posted;
    auto result = beltpp::http::protocol(ssd,
                                         iter_scan_begin,
                                         iter_scan_end,
                                         it_fallback,
                                         10 * 1024,         //  enough length
                                         64 * 1024,         //  header max size
                                         10 * 1024 * 1024,  //  content max size
                                         posted);
    auto code = result.first;
    auto& ss = result.second;

    if (code == beltpp::e_three_state_result::error &&
        ss.status == beltpp::http::detail::scan_status::clean)
    {
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

        if (ss.type == beltpp::http::detail::scan_status::get &&
            ss.resource.path.size() == 1 &&
            ss.resource.path.front() == "storage_order")
        {
            ssd.session_specal_handler = &storage_order_response;

            auto p = ::beltpp::new_void_unique_ptr<StorageUtilityMessage::SignRequest>();
            StorageUtilityMessage::SignRequest& ref = *reinterpret_cast<StorageUtilityMessage::SignRequest*>(p.get());

            ref.private_key =  ss.resource.arguments["private_key"];
            ref.order.storage_address = ss.resource.arguments["storage_address"];
            ref.order.file_uri = ss.resource.arguments["file_uri"];
            ref.order.content_unit_uri = ss.resource.arguments["content_unit_uri"];
            ref.order.session_id = ss.resource.arguments["session_id"];
            ref.order.seconds = SIGN_SECONDS;
            ref.order.time_point.tm = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

            return ::beltpp::detail::pmsg_all(StorageUtilityMessage::SignRequest::rtt,
                                              std::move(p),
                                              &StorageUtilityMessage::SignRequest::pvoid_saver);
        }
        else if (ss.type == beltpp::http::detail::scan_status::get &&
                 ss.resource.path.size() == 2 &&
                 ss.resource.path.front() == "verify_order")
        {
            ssd.session_specal_handler = &storage_verify_response;

            auto p = ::beltpp::new_void_unique_ptr<StorageUtilityMessage::SignedStorageOrder>();
            StorageUtilityMessage::SignedStorageOrder& ref = *reinterpret_cast<StorageUtilityMessage::SignedStorageOrder*>(p.get());

            try
            {
                ref.from_string(meshpp::from_base64(ss.resource.path.back()), putl);

                return ::beltpp::detail::pmsg_all(StorageUtilityMessage::SignedStorageOrder::rtt,
                                                  std::move(p),
                                                  &StorageUtilityMessage::SignedStorageOrder::pvoid_saver);
            }
            catch(...)
            {
                ssd.session_specal_handler = nullptr;

                string message("{\"code\":false,\"reason\":\"invalid storage_order\"}");

                ssd.autoreply = beltpp::http::http_not_found(ssd,
                                                             message);

                return ::beltpp::detail::pmsg_all(size_t(-1),
                                                  ::beltpp::void_unique_nullptr(),
                                                  nullptr);
            }

        }
        else if (ss.type == beltpp::http::detail::scan_status::post &&
                 ss.resource.path.size() == 1 &&
                 ss.resource.path.front() == "api")
        {
            std::string::const_iterator iter_scan_begin_temp = posted.cbegin();
            std::string::const_iterator const iter_scan_end_temp = posted.cend();

            auto parser_unrecognized_limit_backup = ssd.parser_unrecognized_limit;
            ssd.parser_unrecognized_limit = 0;

            auto pmsgall = fallback_message_list_load(iter_scan_begin_temp, iter_scan_end_temp, ssd, putl);

            ssd.parser_unrecognized_limit = parser_unrecognized_limit_backup;

            if (pmsgall.pmsg)
                return pmsgall;

            return protocol_error();
        }
        else if (ss.type == beltpp::http::detail::scan_status::get &&
                 ss.resource.path.size() == 1 &&
                 ss.resource.path.front() == "protocol")
        {
            ssd.session_specal_handler = nullptr;

            ssd.autoreply = beltpp::http::http_response(ssd, StorageUtilityMessage::detail::storage_json_schema());

            return ::beltpp::detail::pmsg_all(size_t(-1),
                                              ::beltpp::void_unique_nullptr(),
                                              nullptr);
        }
        else
        {
            ssd.session_specal_handler = nullptr;

            string message("noo! \r\n");

            for (auto const& dir : ss.resource.path)
                message += "/" + dir;
            message += "\r\n";
            for (auto const& arg : ss.resource.arguments)
                message += (arg.first + ": " + arg.second + "\r\n");
            message += "\r\n";
            message += "\r\n";
            for (auto const& prop : ss.resource.properties)
                message += (prop.first + ": " + prop.second + "\r\n");
            message += "that's an error! \r\n";
            message += "here's the protocol, by the way \r\n";

            ssd.autoreply = beltpp::http::http_not_found(ssd,
                                                         message +
                                                         StorageUtilityMessage::detail::storage_json_schema());

            return ::beltpp::detail::pmsg_all(size_t(-1),
                                              ::beltpp::void_unique_nullptr(),
                                              nullptr);
        }
    }
}
}
}
