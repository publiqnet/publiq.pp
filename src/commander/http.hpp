#pragma once

#include "commander_message.hpp"

#include <belt.pp/global.hpp>
#include <belt.pp/parser.hpp>
#include <belt.pp/http.hpp>

#include <string>
#include <vector>
#include <utility>
#include <unordered_map>

using std::string;
using std::vector;
using std::pair;
using std::unordered_map;

namespace commander
{
namespace http
{

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
        ssd.session_specal_handler = &response;
        ssd.autoreply.clear();

        if (pss->type == beltpp::http::detail::scan_status::get &&
            pss->resource.path.size() == 1 &&
            pss->resource.path.front() == "accounts")
        {
            auto p = ::beltpp::new_void_unique_ptr<CommanderMessage::AccountsRequest>();
            //CommanderMessage::UsersRequest& ref = *reinterpret_cast<CommanderMessage::AccountsRequest*>(p.get());
            ssd.ptr_data = beltpp::t_unique_nullptr<beltpp::detail::iscan_status>();
            return ::beltpp::detail::pmsg_all(CommanderMessage::AccountsRequest::rtt,
                                              std::move(p),
                                              &CommanderMessage::AccountsRequest::pvoid_saver);
        }
        else if (pss->type == beltpp::http::detail::scan_status::get &&
                 pss->resource.path.size() == 2 &&
                 pss->resource.path.front() == "import")
        {
            auto p = ::beltpp::new_void_unique_ptr<CommanderMessage::ImportAccount>();
            CommanderMessage::ImportAccount& ref = *reinterpret_cast<CommanderMessage::ImportAccount*>(p.get());
            ref.address = pss->resource.path.back();

            ssd.ptr_data = beltpp::t_unique_nullptr<beltpp::detail::iscan_status>();
            return ::beltpp::detail::pmsg_all(CommanderMessage::ImportAccount::rtt,
                                              std::move(p),
                                              &CommanderMessage::ImportAccount::pvoid_saver);
        }
        else if (pss->type == beltpp::http::detail::scan_status::get &&
                 pss->resource.path.size() == 4 &&
                 pss->resource.path.front() == "log")
        {
            size_t pos;

            auto p = ::beltpp::new_void_unique_ptr<CommanderMessage::AccountHistoryRequest>();
            CommanderMessage::AccountHistoryRequest& ref = *reinterpret_cast<CommanderMessage::AccountHistoryRequest*>(p.get());
            ref.address = pss->resource.path.back();
            ref.start_block_index = beltpp::stoui64(pss->resource.path[1], pos);
            ref.max_block_count = beltpp::stoui64(pss->resource.path[2], pos);

            ssd.ptr_data = beltpp::t_unique_nullptr<beltpp::detail::iscan_status>();
            return ::beltpp::detail::pmsg_all(CommanderMessage::AccountHistoryRequest::rtt,
                                              std::move(p),
                                              &CommanderMessage::AccountHistoryRequest::pvoid_saver);
        }
        else if (pss->type == beltpp::http::detail::scan_status::get &&
                 pss->resource.path.size() == 1 &&
                 pss->resource.path.front() == "head_block")
        {
            auto p = ::beltpp::new_void_unique_ptr<CommanderMessage::HeadBlockRequest>();
            //CommanderMessage::HeadBlockRequest& ref = *reinterpret_cast<CommanderMessage::HeadBlockRequest*>(p.get());

            ssd.ptr_data = beltpp::t_unique_nullptr<beltpp::detail::iscan_status>();
            return ::beltpp::detail::pmsg_all(CommanderMessage::HeadBlockRequest::rtt,
                                              std::move(p),
                                              &CommanderMessage::HeadBlockRequest::pvoid_saver);
        }
        else if (pss->type == beltpp::http::detail::scan_status::get &&
                 pss->resource.path.size() == 2 &&
                 pss->resource.path.front() == "account")
        {
            auto p = ::beltpp::new_void_unique_ptr<CommanderMessage::AccountRequest>();
            CommanderMessage::AccountRequest& ref = *reinterpret_cast<CommanderMessage::AccountRequest*>(p.get());
            ref.address = pss->resource.path.back();

            ssd.ptr_data = beltpp::t_unique_nullptr<beltpp::detail::iscan_status>();
            return ::beltpp::detail::pmsg_all(CommanderMessage::AccountRequest::rtt,
                                              std::move(p),
                                              &CommanderMessage::AccountRequest::pvoid_saver);
        }
        else if (pss->type == beltpp::http::detail::scan_status::get &&
                 pss->resource.path.size() == 2 &&
                 pss->resource.path.front() == "block")
        {
            auto p = ::beltpp::new_void_unique_ptr<CommanderMessage::BlockInfoRequest>();
            CommanderMessage::BlockInfoRequest& ref = *reinterpret_cast<CommanderMessage::BlockInfoRequest*>(p.get());

            size_t pos;

            ref.block_number = beltpp::stoui64(pss->resource.path.back(), pos);

            ssd.ptr_data = beltpp::t_unique_nullptr<beltpp::detail::iscan_status>();
            return ::beltpp::detail::pmsg_all(CommanderMessage::BlockInfoRequest::rtt,
                                              std::move(p),
                                              &CommanderMessage::BlockInfoRequest::pvoid_saver);
        }
        else if (pss->type == beltpp::http::detail::scan_status::get &&
                 pss->resource.path.size() == 2 &&
                 pss->resource.path.front() == "send")
        {
            auto p = ::beltpp::new_void_unique_ptr<CommanderMessage::Send>();
            CommanderMessage::Send& ref = *reinterpret_cast<CommanderMessage::Send*>(p.get());

            size_t pos;

            ref.private_key = pss->resource.path.back();
            ref.to = pss->resource.arguments["to"];
            ref.amount.whole = beltpp::stoui64(pss->resource.arguments["whole"], pos);
            ref.amount.fraction = beltpp::stoui64(pss->resource.arguments["fraction"], pos);
            ref.fee.whole = beltpp::stoui64(pss->resource.arguments["fee_whole"], pos);
            ref.fee.fraction = beltpp::stoui64(pss->resource.arguments["fee_fraction"], pos);
            ref.message = pss->resource.arguments["message"];
            ref.seconds_to_expire = beltpp::stoui64(pss->resource.arguments["seconds"], pos);

            ssd.ptr_data = beltpp::t_unique_nullptr<beltpp::detail::iscan_status>();
            return ::beltpp::detail::pmsg_all(CommanderMessage::Send::rtt,
                                              std::move(p),
                                              &CommanderMessage::Send::pvoid_saver);
        }
        else if (pss->type == beltpp::http::detail::scan_status::get &&
                 pss->resource.path.size() == 1 &&
                 pss->resource.path.front() == "protocol")
        {
            ssd.session_specal_handler = nullptr;

            ssd.autoreply = beltpp::http::http_response(ssd, CommanderMessage::detail::storage<>::json_schema);

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
                                                         CommanderMessage::detail::storage<>::json_schema);

            ssd.ptr_data = beltpp::t_unique_nullptr<beltpp::detail::iscan_status>();

            return ::beltpp::detail::pmsg_all(size_t(-1),
                                              ::beltpp::void_unique_nullptr(),
                                              nullptr);
        }
    }
}
}
}
