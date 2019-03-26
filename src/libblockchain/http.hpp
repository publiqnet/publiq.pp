#pragma once

#include "message.hpp"

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

namespace publiqpp
{
namespace http
{
inline
string response(beltpp::detail::session_special_data& ssd,
                beltpp::packet const& pc)
{
    return beltpp::http::http_response(ssd, pc.to_string());
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
            pss->resource.path.front() == "storage")
        {
            ssd.session_specal_handler = &file_response;

            auto p = ::beltpp::new_void_unique_ptr<BlockchainMessage::StorageFileRequest>();
            BlockchainMessage::StorageFileRequest& ref = *reinterpret_cast<BlockchainMessage::StorageFileRequest*>(p.get());
            ref.uri = pss->resource.arguments["file"];
            ref.channel_address = pss->resource.arguments["channel_address"];
            ssd.ptr_data = beltpp::t_unique_nullptr<beltpp::detail::iscan_status>();
            return ::beltpp::detail::pmsg_all(BlockchainMessage::StorageFileRequest::rtt,
                                              std::move(p),
                                              &BlockchainMessage::StorageFileRequest::pvoid_saver);
        }
        else if (pss->type == beltpp::http::detail::scan_status::post &&
                 pss->resource.path.size() == 1 &&
                 pss->resource.path.front() == "storage")
        {
            auto p = ::beltpp::new_void_unique_ptr<BlockchainMessage::StorageFile>();
            BlockchainMessage::StorageFile& ref = *reinterpret_cast<BlockchainMessage::StorageFile*>(p.get());
            ref.mime_type = pss->resource.properties["Content-Type"];

            ssd.ptr_data = beltpp::t_unique_nullptr<beltpp::detail::iscan_status>();

            ref.data = std::move(posted);

            return ::beltpp::detail::pmsg_all(BlockchainMessage::StorageFile::rtt,
                                              std::move(p),
                                              &BlockchainMessage::StorageFile::pvoid_saver);
        }
        else if (pss->type == beltpp::http::detail::scan_status::get &&
                 pss->resource.path.size() == 2 &&
                 pss->resource.path.front() == "send")
        {
            auto p = ::beltpp::new_void_unique_ptr<BlockchainMessage::TransactionBroadcastRequest>();
            BlockchainMessage::TransactionBroadcastRequest& ref = *reinterpret_cast<BlockchainMessage::TransactionBroadcastRequest*>(p.get());

            size_t pos;
            ref.private_key = pss->resource.path.back();
            meshpp::private_key pv(ref.private_key);

            BlockchainMessage::Transfer transfer;
            transfer.amount.whole = beltpp::stoui64(pss->resource.arguments["whole"], pos);
            transfer.amount.fraction = beltpp::stoui64(pss->resource.arguments["fraction"], pos);
            transfer.from = pv.get_public_key().to_string();
            transfer.message = pss->resource.arguments["message"];
            transfer.to = pss->resource.arguments["to"];

            ref.transaction_details.creation.tm = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            ref.transaction_details.expiry.tm = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() + std::chrono::seconds(beltpp::stoui64(pss->resource.arguments["seconds"], pos)));
            ref.transaction_details.fee.whole = beltpp::stoui64(pss->resource.arguments["fee_whole"], pos);
            ref.transaction_details.fee.fraction = beltpp::stoui64(pss->resource.arguments["fee_fraction"], pos);
            ref.transaction_details.action = std::move(transfer);

            ssd.ptr_data = beltpp::t_unique_nullptr<beltpp::detail::iscan_status>();
            return ::beltpp::detail::pmsg_all(BlockchainMessage::TransactionBroadcastRequest::rtt,
                                              std::move(p),
                                              &BlockchainMessage::TransactionBroadcastRequest::pvoid_saver);
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
                 pss->resource.path.size() == 3 &&
                 pss->resource.path.front() == "key")
        {
            size_t pos;

            auto p = ::beltpp::new_void_unique_ptr<BlockchainMessage::KeyPairRequest>();
            BlockchainMessage::KeyPairRequest& ref = *reinterpret_cast<BlockchainMessage::KeyPairRequest*>(p.get());

            ref.master_key = pss->resource.path[1];
            ref.index = beltpp::stoui64(pss->resource.path[2], pos);

            ssd.ptr_data = beltpp::t_unique_nullptr<beltpp::detail::iscan_status>();
            return ::beltpp::detail::pmsg_all(BlockchainMessage::KeyPairRequest::rtt,
                                              std::move(p),
                                              &BlockchainMessage::KeyPairRequest::pvoid_saver);
        }
        else if (pss->type == beltpp::http::detail::scan_status::get &&
                 pss->resource.path.size() == 1 &&
                 pss->resource.path.front() == "protocol")
        {
            ssd.session_specal_handler = nullptr;

            ssd.autoreply = beltpp::http::http_response(ssd, BlockchainMessage::detail::storage<>::json_schema);

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
                                                         BlockchainMessage::detail::storage<>::json_schema);

            ssd.ptr_data = beltpp::t_unique_nullptr<beltpp::detail::iscan_status>();

            return ::beltpp::detail::pmsg_all(size_t(-1),
                                              ::beltpp::void_unique_nullptr(),
                                              nullptr);
        }
    }
}
}
}
