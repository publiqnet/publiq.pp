#include "rpc.hpp"
#include "http.hpp"
#include "daemon_rpc.hpp"
#include "utility.hpp"

#include <belt.pp/scope_helper.hpp>

#include <mesh.pp/settings.hpp>
#include <mesh.pp/cryptoutility.hpp>

#include <publiq.pp/coin.hpp>

#include <algorithm>
#include <memory>
#include <chrono>
#include <unordered_set>
#include <set>
#include <map>

using std::string;
using std::unordered_set;
using std::set;
using std::unique_ptr;
namespace chrono = std::chrono;
using beltpp::packet;
using peer_id = beltpp::socket::peer_id;
using chrono::system_clock;

using namespace ManagerMessage;

using sf = beltpp::socket_family_t<&manager::http::message_list_load<&ManagerMessage::message_list_load>>;

static inline
beltpp::void_unique_ptr get_putl()
{
    beltpp::message_loader_utility utl;
    ManagerMessage::detail::extension_helper(utl);

    auto ptr_utl =
        beltpp::new_void_unique_ptr<beltpp::message_loader_utility>(std::move(utl));

    return ptr_utl;
}

rpc::rpc(string const& str_pv_key,
         beltpp::ip_address const& rpc_address,
         beltpp::ip_address const& connect_to_address,
         uint64_t sync_interval)
    : m_str_pv_key(str_pv_key)
    , eh(beltpp::libsocket::construct_event_handler())
    , rpc_socket(beltpp::libsocket::getsocket<sf>(*eh))
    , head_block_index(meshpp::data_file_path("head_block_index.txt"))
    , blocks("block", meshpp::data_directory_path("blocks"), 10000, 1, get_putl())
    , storages("storages", meshpp::data_directory_path("storages"), 100, get_putl())
    , files("files", meshpp::data_directory_path("files"), 10000, get_putl())
    , connect_to_address(connect_to_address)
{
    eh->set_timer(chrono::seconds(sync_interval));
    eh->add(*rpc_socket);

    m_storage_update_timer.set(chrono::seconds(600));
    m_storage_update_timer.update();

    rpc_socket->listen(rpc_address);
}

void import_account_if_needed(string const& /*address*/,
                              rpc& /*rpc_server*/,
                              beltpp::ip_address const& /*connect_to_address*/)
{
    //Account account;
    //account.address = address;
    //
    //meshpp::public_key pb(account.address);
    //
    //if (false == rpc_server.accounts.contains(address))
    //{
    //    daemon_rpc dm;
    //    dm.open(connect_to_address);
    //    beltpp::finally finally_close([&dm]{ dm.close(); });
    //
    //    auto context_new_import = dm.start_new_import(rpc_server, address);
    //    auto context_sync = dm.start_sync(rpc_server, rpc_server.accounts.keys());
    //    // if in new import case sync will not reach the same index as in regular import case
    //    // it will call regular sync untill reach same log index
    //    do
    //    {
    //        dm.sync(rpc_server, context_new_import);
    //        dm.sync(rpc_server, context_sync);
    //    }
    //    while (context_sync.start_index() != context_new_import.start_index());
    //
    //    context_new_import.save();
    //    context_sync.save();
    //
    //    context_new_import.commit();
    //    context_sync.commit();
    //}
    //
    //if (false == rpc_server.accounts.contains(address))
    //{
    //    beltpp::on_failure guard([&rpc_server](){rpc_server.accounts.discard();});
    //    rpc_server.accounts.insert(address, account);
    //    rpc_server.accounts.save();
    //
    //    guard.dismiss();
    //    rpc_server.accounts.commit();
    //}
}

beltpp::packet send(Send const& send,
                    rpc& rpc_server,
                    beltpp::ip_address const& connect_to_address)
{
    daemon_rpc dm;
    dm.open(connect_to_address);
    beltpp::finally finally_close([&dm]{ dm.close(); });

    return dm.send(send, rpc_server);
}

beltpp::packet process_storage_update_request(StorageUpdateRequest const& update,
                                              rpc& rpc_server,
                                              beltpp::ip_address const& connect_to_address)
{
    daemon_rpc dm;
    dm.open(connect_to_address);
    beltpp::finally finally_close([&dm]{ dm.close(); });

    return dm.process_storage_update_request(update, rpc_server);
}

void rpc::run()
{
    unordered_set<beltpp::event_item const*> wait_sockets;
    auto wait_result = eh->wait(wait_sockets);
    B_UNUSED(wait_sockets);

    if (wait_result & beltpp::event_handler::event)
    {
        beltpp::stream::peer_id peerid;
        beltpp::socket::packets received_packets;

        received_packets = rpc_socket->receive(peerid);

        for (auto& received_packet : received_packets)
        {
        try
        {
            packet& ref_packet = received_packet;

            switch (ref_packet.type())
            {
            case ImportStorage::rtt:
            {
                ImportStorage msg;
                std::move(ref_packet).get(msg);

                import_account_if_needed(msg.address, *this, connect_to_address);

                rpc_socket->send(peerid, beltpp::packet(Done()));
                break;
            }
            case HeadBlockRequest::rtt:
            {
                NumberValue response;
                response.value = head_block_index.as_const()->value;

                rpc_socket->send(peerid, beltpp::packet(response));
                break;
            }
            case BlockInfoRequest::rtt:
            {
                BlockInfoRequest msg;
                std::move(ref_packet).get(msg);

                BlockInfo response;

                auto blocks_size = blocks.size();
                if (msg.block_number < blocks.size())
                    response = blocks.as_const().at(msg.block_number);
                else
                    response = blocks.as_const().at(blocks_size - 1);

                rpc_socket->send(peerid, beltpp::packet(response));

                break;
            }
            case Send::rtt:
            {
                Send msg;
                std::move(ref_packet).get(msg);

                meshpp::private_key pv(msg.private_key);

                import_account_if_needed(pv.get_public_key().to_string(),
                                         *this,
                                         connect_to_address);

                rpc_socket->send(peerid, send(msg, *this, connect_to_address));

                break;
            }
            case StoragesRequest::rtt:
            {
                StoragesResponse response;
                for (auto const& storage : storages.keys())
                         response.storages.push_back(storages.as_const().at(storage));

                rpc_socket->send(peerid, beltpp::packet(response));
                break;
            }
            case StorageUpdateRequest::rtt:
            {
                StorageUpdateRequest msg;
                std::move(ref_packet).get(msg);

                rpc_socket->send(peerid, process_storage_update_request(msg, *this, connect_to_address));

                break;
            }
            case Failed::rtt:
            {
                rpc_socket->send(peerid, std::move(ref_packet));

                break;
            }
            }
        }
        catch (meshpp::exception_private_key const& ex)
        {
            BlockchainMessage::InvalidPrivateKey reason;
            reason.private_key = ex.priv_key;

            Failed msg;
            msg.reason = std::move(reason);
            msg.message = ex.what();
            rpc_socket->send(peerid, beltpp::packet(std::move(msg)));
            throw;
        }
        catch(std::exception const& ex)
        {
            Failed msg;
            msg.message = ex.what();
            rpc_socket->send(peerid, beltpp::packet(std::move(msg)));
            throw;
        }
        catch(...)
        {
            Failed msg;
            msg.message = "unknown error";
            rpc_socket->send(peerid, beltpp::packet(std::move(msg)));
            throw;
        }
        }
    }

    if (wait_result & beltpp::event_handler::timer_out)
    {
        daemon_rpc dm;
        dm.open(connect_to_address);
        beltpp::finally finally_close([&dm]{ dm.close(); });

        // sync data from node
        //auto context_sync = dm.start_sync(*this, accounts.keys());

        //dm.sync(*this, context_sync);

        //context_sync.save();
        //context_sync.commit();

        // send broadcast packet with storage management command
        //if (false == m_str_pv_key.empty() &&
        //    m_storage_update_timer.expired())
        //{
        //    m_storage_update_timer.update();
        //
        //    unordered_map<string, uint64_t> usage_map;
        //    uint64_t block_number = head_block_index.as_const()->value;
        //
        //    for (auto index = block_number; index > 0 && index > block_number - 144; --index)
        //        for (auto const& item : m_file_usage_map[index])
        //            usage_map[item.first] += item.second;
        //
        //    for (auto it = m_file_usage_map.begin(); it != m_file_usage_map.end(); )
        //        if (it->first < block_number - 144)
        //            it = m_file_usage_map.erase(it);
        //        else
        //            ++it;
        //
        //    std::multimap<uint64_t, string> ordered_map;
        //    meshpp::private_key pv_key = meshpp::private_key(m_str_pv_key);
        //    
        //    for (auto it = usage_map.begin(); it != usage_map.end(); ++it)
        //        ordered_map.insert({ it->second, it->first });
        //
        //    auto count = ordered_map.size();
        //    count = count == 0 ? 0 : 1 + 2 * count / 3;
        //
        //    auto it = ordered_map.rbegin();
        //    while (count > 0 && it != ordered_map.rend())
        //    {
        //        auto search_result = search_file(*this, it->second);
        //
        //        for( auto const& item : search_result)
        //        {
        //            BlockchainMessage::StorageUpdateCommand update_command;
        //            update_command.status = BlockchainMessage::UpdateType::store;
        //            update_command.file_uri = it->second;
        //            update_command.storage_address = item.first;
        //            update_command.channel_address = item.second;
        //
        //            BlockchainMessage::Transaction transaction;
        //            transaction.action = std::move(update_command);
        //            transaction.creation.tm = system_clock::to_time_t(system_clock::now());
        //            transaction.expiry.tm = system_clock::to_time_t(system_clock::now() + chrono::seconds(24 * 3600));
        //
        //            BlockchainMessage::Authority authorization;
        //            authorization.address = pv_key.get_public_key().to_string();
        //            authorization.signature = pv_key.sign(transaction.to_string()).base58;
        //
        //            BlockchainMessage::SignedTransaction signed_transaction;
        //            signed_transaction.authorizations.push_back(authorization);
        //            signed_transaction.transaction_details = transaction;
        //
        //            BlockchainMessage::Broadcast broadcast;
        //            broadcast.echoes = 2;
        //            broadcast.package = signed_transaction;
        //
        //            dm.socket->send(dm.peerid, beltpp::packet(broadcast));
        //            dm.wait_response(string());
        //        }
        //
        //        ++it;
        //        --count;
        //    }
        //}
    }
}
