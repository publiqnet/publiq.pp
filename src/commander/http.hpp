#pragma once

#include "commander_message.hpp"

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

namespace commander
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

beltpp::detail::pmsg_all request_failed(string const& message,
                                        beltpp::detail::session_special_data& ssd)
{
    auto p = ::beltpp::new_void_unique_ptr<CommanderMessage::Failed>();
    CommanderMessage::Failed & ref = *reinterpret_cast<CommanderMessage::Failed*>(p.get());
    ref.message = message;
    return ::beltpp::detail::pmsg_all(CommanderMessage::Failed::rtt,
                                      std::move(p),
                                      &CommanderMessage::Failed::pvoid_saver);
}

inline
string response(beltpp::detail::session_special_data& ssd,
                beltpp::packet const& pc)
{
    if (pc.type() == CommanderMessage::Failed::rtt)
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
                                         1000,
                                         64 * 1024,
                                         10 * 1024 * 1024,
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
            ss.resource.path.size() == 1 &&
            ss.resource.path.front() == "accounts")
        {
            auto p = ::beltpp::new_void_unique_ptr<CommanderMessage::AccountsRequest>();
            //CommanderMessage::UsersRequest& ref = *reinterpret_cast<CommanderMessage::AccountsRequest*>(p.get());
            return ::beltpp::detail::pmsg_all(CommanderMessage::AccountsRequest::rtt,
                                              std::move(p),
                                              &CommanderMessage::AccountsRequest::pvoid_saver);
        }
        else if (ss.type == beltpp::http::detail::scan_status::get &&
                 ss.resource.path.size() == 2 &&
                 ss.resource.path.front() == "import")
        {
            auto p = ::beltpp::new_void_unique_ptr<CommanderMessage::ImportAccount>();
            CommanderMessage::ImportAccount& ref = *reinterpret_cast<CommanderMessage::ImportAccount*>(p.get());
            ref.address = ss.resource.path.back();

            return ::beltpp::detail::pmsg_all(CommanderMessage::ImportAccount::rtt,
                                              std::move(p),
                                              &CommanderMessage::ImportAccount::pvoid_saver);
        }
        else if (ss.type == beltpp::http::detail::scan_status::get &&
                 ss.resource.path.size() == 4 &&
                 ss.resource.path.front() == "log")
        {
            size_t pos;

            auto p = ::beltpp::new_void_unique_ptr<CommanderMessage::AccountHistoryRequest>();
            CommanderMessage::AccountHistoryRequest& ref = *reinterpret_cast<CommanderMessage::AccountHistoryRequest*>(p.get());
            ref.address = ss.resource.path.back();

            std::vector<string> args {ss.resource.path[1], ss.resource.path[2]};
            if (ss.resource.path[1].empty() ||
                ss.resource.path[2].empty())
                return request_failed("invalid argument: ", ssd);

            for (auto const& it : args)
            {
                beltpp::stoui64(it, pos);
                if (it.size() != pos)
                    return request_failed("invalid argument: " + it, ssd);
            }

            ref.start_block_index = beltpp::stoui64(ss.resource.path[1], pos);
            ref.max_block_count = beltpp::stoui64(ss.resource.path[2], pos);

            return ::beltpp::detail::pmsg_all(CommanderMessage::AccountHistoryRequest::rtt,
                                              std::move(p),
                                              &CommanderMessage::AccountHistoryRequest::pvoid_saver);
        }
        else if (ss.type == beltpp::http::detail::scan_status::get &&
                 ss.resource.path.size() == 1 &&
                 ss.resource.path.front() == "head_block")
        {
            auto p = ::beltpp::new_void_unique_ptr<CommanderMessage::HeadBlockRequest>();
            //CommanderMessage::HeadBlockRequest& ref = *reinterpret_cast<CommanderMessage::HeadBlockRequest*>(p.get());

            return ::beltpp::detail::pmsg_all(CommanderMessage::HeadBlockRequest::rtt,
                                              std::move(p),
                                              &CommanderMessage::HeadBlockRequest::pvoid_saver);
        }
        else if (ss.type == beltpp::http::detail::scan_status::get &&
                 ss.resource.path.size() == 2 &&
                 ss.resource.path.front() == "account")
        {
            auto p = ::beltpp::new_void_unique_ptr<CommanderMessage::AccountRequest>();
            CommanderMessage::AccountRequest& ref = *reinterpret_cast<CommanderMessage::AccountRequest*>(p.get());
            ref.address = ss.resource.path.back();

            return ::beltpp::detail::pmsg_all(CommanderMessage::AccountRequest::rtt,
                                              std::move(p),
                                              &CommanderMessage::AccountRequest::pvoid_saver);
        }
        else if (ss.type == beltpp::http::detail::scan_status::get &&
                 ss.resource.path.size() == 2 &&
                 ss.resource.path.front() == "block")
        {
            auto p = ::beltpp::new_void_unique_ptr<CommanderMessage::BlockInfoRequest>();
            CommanderMessage::BlockInfoRequest& ref = *reinterpret_cast<CommanderMessage::BlockInfoRequest*>(p.get());

            size_t pos;
            std::vector<string> args {ss.resource.path.back()};
            if (ss.resource.path.back().empty())
                return request_failed("invalid argument: ", ssd);

            for (auto const& it : args)
            {
                beltpp::stoui64(it, pos);
                if (it.size() != pos)
                    return request_failed("invalid argument: " + it, ssd);
            }

            ref.block_number = beltpp::stoui64(ss.resource.path.back(), pos);

            return ::beltpp::detail::pmsg_all(CommanderMessage::BlockInfoRequest::rtt,
                                              std::move(p),
                                              &CommanderMessage::BlockInfoRequest::pvoid_saver);
        }
        else if (ss.type == beltpp::http::detail::scan_status::get &&
                 ss.resource.path.size() == 3 &&
                 ss.resource.path.front() == "miners")
        {
            auto p = ::beltpp::new_void_unique_ptr<CommanderMessage::MinersRequest>();
            CommanderMessage::MinersRequest& ref = *reinterpret_cast<CommanderMessage::MinersRequest*>(p.get());

            size_t pos;
            ref.start_block_index = beltpp::stoui64(ss.resource.path[1], pos);
            ref.end_block_index = beltpp::stoui64(ss.resource.path[2], pos);

            if (ref.start_block_index > ref.end_block_index)
                return request_failed("invalid arguments: ", ssd);

            return ::beltpp::detail::pmsg_all(CommanderMessage::MinersRequest::rtt,
                                              std::move(p),
                                              &CommanderMessage::MinersRequest::pvoid_saver);
        }
        else if (ss.type == beltpp::http::detail::scan_status::get &&
                 ss.resource.path.size() == 2 &&
                 ss.resource.path.front() == "send")
        {
            const std::set<string> all_arguments = {"to", "whole", "fraction", "fee_whole", "fee_fraction", "message", "seconds"};
            const std::set<string> ui64_arguments = {"whole", "fraction", "fee_whole", "fee_fraction", "seconds"};

            string check_result = check_arguments(ss.resource.arguments, all_arguments, ui64_arguments);
            if (!check_result.empty())
                return request_failed(check_result, ssd);

            auto p = ::beltpp::new_void_unique_ptr<CommanderMessage::Send>();
            CommanderMessage::Send& ref = *reinterpret_cast<CommanderMessage::Send*>(p.get());

            size_t pos;

            ref.private_key = ss.resource.path.back();
            ref.to = ss.resource.arguments["to"];
            ref.amount.whole = beltpp::stoui64(ss.resource.arguments["whole"], pos);
            ref.amount.fraction = beltpp::stoui64(ss.resource.arguments["fraction"], pos);
            ref.fee.whole = beltpp::stoui64(ss.resource.arguments["fee_whole"], pos);
            ref.fee.fraction = beltpp::stoui64(ss.resource.arguments["fee_fraction"], pos);
            ref.message = ss.resource.arguments["message"];
            ref.seconds_to_expire = beltpp::stoui64(ss.resource.arguments["seconds"], pos);

            return ::beltpp::detail::pmsg_all(CommanderMessage::Send::rtt,
                                              std::move(p),
                                              &CommanderMessage::Send::pvoid_saver);
        }
        else if (ss.type == beltpp::http::detail::scan_status::get &&
                 ss.resource.path.size() == 1 &&
                 ss.resource.path.front() == "storages")
        {
            auto p = ::beltpp::new_void_unique_ptr<CommanderMessage::StoragesRequest>();
            //CommanderMessage::StoragesRequest& ref = *reinterpret_cast<CommanderMessage::StoragesRequest*>(p.get());

            return ::beltpp::detail::pmsg_all(CommanderMessage::StoragesRequest::rtt,
                                              std::move(p),
                                              &CommanderMessage::StoragesRequest::pvoid_saver);
        }
        else if (ss.type == beltpp::http::detail::scan_status::get &&
                 ss.resource.path.size() == 2 &&
                 ss.resource.path.front() == "file")
        {
            const std::set<string> all_arguments = {"private_key", "status", "file_uri", "storage_address", "fee_whole", "fee_fraction", "message", "seconds"};
            const std::set<string> ui64_arguments = {"fee_whole", "fee_fraction", "seconds"};

            string check_result = check_arguments(ss.resource.arguments, all_arguments, ui64_arguments);
            if (!check_result.empty())
                return request_failed(check_result, ssd);

            auto p = ::beltpp::new_void_unique_ptr<CommanderMessage::StorageUpdateRequest>();
            CommanderMessage::StorageUpdateRequest& ref = *reinterpret_cast<CommanderMessage::StorageUpdateRequest*>(p.get());

            size_t pos;

            ref.private_key = ss.resource.path.back();
            ref.status = ss.resource.arguments["status"];
            ref.file_uri = ss.resource.arguments["file_uri"];
            ref.storage_address = ss.resource.arguments["storage_address"];
            ref.fee.whole = beltpp::stoui64(ss.resource.arguments["fee_whole"], pos);
            ref.fee.fraction = beltpp::stoui64(ss.resource.arguments["fee_fraction"], pos);
            ref.message = ss.resource.arguments["message"];
            ref.seconds_to_expire = beltpp::stoui64(ss.resource.arguments["seconds"], pos);

            return ::beltpp::detail::pmsg_all(CommanderMessage::StorageUpdateRequest::rtt,
                                              std::move(p),
                                              &CommanderMessage::StorageUpdateRequest::pvoid_saver);
        }
        else if (ss.type == beltpp::http::detail::scan_status::get &&
                 ss.resource.path.size() == 1 &&
                 ss.resource.path.front() == "champions")
        {
            auto p = ::beltpp::new_void_unique_ptr<CommanderMessage::ChampionMinersRequest>();
            //CommanderMessage::ChampionMinersRequest& ref = *reinterpret_cast<CommanderMessage::ChampionMinersRequest*>(p.get());

            return ::beltpp::detail::pmsg_all(CommanderMessage::ChampionMinersRequest::rtt,
                                                      std::move(p),
                                                      &CommanderMessage::ChampionMinersRequest::pvoid_saver);
        }
        else if (ss.type == beltpp::http::detail::scan_status::get &&
                 ss.resource.path.size() == 1 &&
                 ss.resource.path.front() == "channels")
        {
            auto p = ::beltpp::new_void_unique_ptr<CommanderMessage::ChannelsRequest>();
            //CommanderMessage::ChannelsRequest& ref = *reinterpret_cast<CommanderMessage::ChannelsRequest*>(p.get());

            return ::beltpp::detail::pmsg_all(CommanderMessage::ChannelsRequest::rtt,
                                                      std::move(p),
                                                      &CommanderMessage::ChannelsRequest::pvoid_saver);
        }
        else if (ss.type == beltpp::http::detail::scan_status::get &&
                 ss.resource.path.size() == 1 &&
                 ss.resource.path.front() == "protocol")
        {
            ssd.session_specal_handler = nullptr;

            ssd.autoreply = beltpp::http::http_response(ssd, CommanderMessage::detail::storage_json_schema());

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
                                                         CommanderMessage::detail::storage_json_schema());

            return ::beltpp::detail::pmsg_all(size_t(-1),
                                              ::beltpp::void_unique_nullptr(),
                                              nullptr);
        }
    }
}
}
}
