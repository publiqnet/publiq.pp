#pragma once

#include "storage_manager_message.hpp"

#include <belt.pp/global.hpp>
#include <belt.pp/parser.hpp>
#include <belt.pp/http.hpp>

#include <string>
#include <vector>
#include <utility>
#include <unordered_map>
#include <set>

using std::string;
using std::vector;
using std::pair;
using std::unordered_map;

namespace manager
{
namespace http
{
string check_arguments(unordered_map<string, string>& arguments,
                       std::set<string> const& all_arguments,
                       std::set<string> const& ui64_arguments)
{
    for (auto const& it : arguments)
        if (it.second.empty() ||
            all_arguments.find(it.first) == all_arguments.end())
                return "invalid argument: " + it.first;

    size_t pos;
    for (auto const& it : ui64_arguments)
        if (arguments.find(it) != arguments.end())
        {
            beltpp::stoui64(arguments[it], pos);
            if (arguments[it].size() != pos)
                return "invalid argument: " + it + " " + arguments[it];
        }

    return string();
}

beltpp::detail::pmsg_all request_failed(string const& message)
{
    auto p = ::beltpp::new_void_unique_ptr<ManagerMessage::Failed>();
    ManagerMessage::Failed & ref = *reinterpret_cast<ManagerMessage::Failed*>(p.get());
    ref.message = message;
    return ::beltpp::detail::pmsg_all(ManagerMessage::Failed::rtt,
                                      std::move(p),
                                      &ManagerMessage::Failed::pvoid_saver);
}

inline
string response(beltpp::detail::session_special_data& ssd,
                beltpp::packet const& pc)
{
    if (pc.type() == ManagerMessage::Failed::rtt)
        return beltpp::http::http_not_found(ssd, pc.to_string());
    else
        return beltpp::http::http_response(ssd, pc.to_string());
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
        ssd.session_specal_handler = &response;
        ssd.autoreply.clear();

        if (ss.type == beltpp::http::detail::scan_status::get &&
            ss.resource.path.size() == 2 &&
            ss.resource.path.front() == "importstorage")
        {
            auto p = ::beltpp::new_void_unique_ptr<ManagerMessage::ImportStorage>();
            ManagerMessage::ImportStorage& ref = *reinterpret_cast<ManagerMessage::ImportStorage*>(p.get());
            ref.address = ss.resource.path.back();

            return ::beltpp::detail::pmsg_all(ManagerMessage::ImportStorage::rtt,
                                              std::move(p),
                                              &ManagerMessage::ImportStorage::pvoid_saver);
        }
        else if (ss.type == beltpp::http::detail::scan_status::get &&
                 ss.resource.path.size() == 1 &&
                 ss.resource.path.front() == "head_block")
        {
            auto p = ::beltpp::new_void_unique_ptr<ManagerMessage::HeadBlockRequest>();

            return ::beltpp::detail::pmsg_all(ManagerMessage::HeadBlockRequest::rtt,
                                              std::move(p),
                                              &ManagerMessage::HeadBlockRequest::pvoid_saver);
        }
        else if (ss.type == beltpp::http::detail::scan_status::get &&
                 ss.resource.path.size() == 1 &&
                 ss.resource.path.front() == "storages")
        {
            auto p = ::beltpp::new_void_unique_ptr<ManagerMessage::StoragesRequest>();

            return ::beltpp::detail::pmsg_all(ManagerMessage::StoragesRequest::rtt,
                                              std::move(p),
                                              &ManagerMessage::StoragesRequest::pvoid_saver);
        }
        else if (ss.type == beltpp::http::detail::scan_status::get &&
                 ss.resource.path.size() == 2 &&
                 ss.resource.path.front() == "file")
        {
            const std::set<string> all_arguments = {"private_key", "status", "file_uri", "storage_address", "fee_whole", "fee_fraction", "message", "seconds"};
            const std::set<string> ui64_arguments = {"fee_whole", "fee_fraction", "seconds"};

            string check_result = check_arguments(ss.resource.arguments, all_arguments, ui64_arguments);
            if (!check_result.empty())
                return request_failed(check_result);

            auto p = ::beltpp::new_void_unique_ptr<ManagerMessage::StorageUpdateRequest>();
            ManagerMessage::StorageUpdateRequest& ref = *reinterpret_cast<ManagerMessage::StorageUpdateRequest*>(p.get());

            size_t pos;

            ref.private_key = ss.resource.path.back();
            ref.status = ss.resource.arguments["status"];
            ref.file_uri = ss.resource.arguments["file_uri"];
            ref.storage_address = ss.resource.arguments["storage_address"];
            ref.fee.whole = beltpp::stoui64(ss.resource.arguments["fee_whole"], pos);
            ref.fee.fraction = beltpp::stoui64(ss.resource.arguments["fee_fraction"], pos);
            ref.message = ss.resource.arguments["message"];
            ref.seconds_to_expire = beltpp::stoui64(ss.resource.arguments["seconds"], pos);

            return ::beltpp::detail::pmsg_all(ManagerMessage::StorageUpdateRequest::rtt,
                                              std::move(p),
                                              &ManagerMessage::StorageUpdateRequest::pvoid_saver);
        }
        else if (ss.type == beltpp::http::detail::scan_status::get &&
                 ss.resource.path.size() == 1 &&
                 ss.resource.path.front() == "protocol")
        {
            ssd.session_specal_handler = nullptr;

            ssd.autoreply = beltpp::http::http_response(ssd, ManagerMessage::detail::storage_json_schema());

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
                                                         ManagerMessage::detail::storage_json_schema());

            return ::beltpp::detail::pmsg_all(size_t(-1),
                                              ::beltpp::void_unique_nullptr(),
                                              nullptr);
        }
    }
}
}
}
