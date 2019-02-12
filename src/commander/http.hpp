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

namespace commander
{
namespace http
{
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
        //if (pss->type == meshpp::http::detail::scan_status::post)
        {
            ssd.session_specal_handler = &meshpp::http::http_response;
            ssd.autoreply.clear();
        }

        if (pss->type == meshpp::http::detail::scan_status::get &&
            pss->resource.path.size() == 1 &&
            pss->resource.path.front() == "users")
        {
            auto p = ::beltpp::new_void_unique_ptr<CommanderMessage::UsersRequest>();
            //CommanderMessage::UsersRequest& ref = *reinterpret_cast<CommanderMessage::UsersRequest*>(p.get());
            ssd.ptr_data = beltpp::t_unique_nullptr<beltpp::detail::iscan_status>();
            return ::beltpp::detail::pmsg_all(CommanderMessage::UsersRequest::rtt,
                                              std::move(p),
                                              &CommanderMessage::UsersRequest::pvoid_saver);
        }
        else
            return protocol_error();
    }
}
}
}
