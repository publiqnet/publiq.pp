#include "manager.hpp"
#include "http.hpp"
#include "daemon_manager.hpp"
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
#include <iostream>
#include <thread>

using std::set;
using std::string;
using std::multimap;
using std::unique_ptr;
using std::unordered_set;
namespace chrono = std::chrono;

using beltpp::packet;
using peer_id = beltpp::socket::peer_id;
using chrono::system_clock;

using namespace ManagerMessage;

using sf = beltpp::socket_family_t<&storage_manager::http::message_list_load<&ManagerMessage::message_list_load>>;

static inline
beltpp::void_unique_ptr get_putl()
{
    beltpp::message_loader_utility utl;
    ManagerMessage::detail::extension_helper(utl);

    auto ptr_utl =
        beltpp::new_void_unique_ptr<beltpp::message_loader_utility>(std::move(utl));

    return ptr_utl;
}

manager::manager(string const& str_pv_key,
                 beltpp::ip_address const& rpc_address,
                 beltpp::ip_address const& connect_to_address,
                 uint64_t sync_interval)
    : m_str_pv_key(str_pv_key)
    , eh(beltpp::libsocket::construct_event_handler())
    , rpc_socket(beltpp::libsocket::getsocket<sf>(*eh))
    , files("files", meshpp::data_directory_path("files"), 10000, get_putl())
    , storages("storages", meshpp::data_directory_path("storages"), 100, get_putl())
    , head_block_index(meshpp::data_file_path("head_block_index.txt"))
    , connect_to_address(connect_to_address)
{
    eh->set_timer(chrono::seconds(sync_interval));
    eh->add(*rpc_socket);

    storage_update_timer.set(chrono::seconds(600));
    storage_update_timer.update();

    rpc_socket->listen(rpc_address);
}

void send_command(meshpp::private_key const& pv_key,
                  string const& file_uri,
                  string const& storage_address,
                  string const& channel_address,
                  sm_daemon& dm)
{
    BlockchainMessage::StorageUpdateCommand update_command;
    update_command.status = BlockchainMessage::UpdateType::store;
    update_command.file_uri = file_uri;
    update_command.storage_address = storage_address;
    update_command.channel_address = channel_address;

    BlockchainMessage::Transaction transaction;
    transaction.action = std::move(update_command);
    transaction.creation.tm = system_clock::to_time_t(system_clock::now());
    transaction.expiry.tm = system_clock::to_time_t(system_clock::now() + chrono::seconds(24 * 3600));

    BlockchainMessage::Authority authorization;
    authorization.address = pv_key.get_public_key().to_string();
    authorization.signature = pv_key.sign(transaction.to_string()).base58;

    BlockchainMessage::SignedTransaction signed_transaction;
    signed_transaction.authorizations.push_back(authorization);
    signed_transaction.transaction_details = transaction;

    BlockchainMessage::Broadcast broadcast;
    broadcast.echoes = 2;
    broadcast.package = signed_transaction;

    dm.socket->send(dm.peerid, beltpp::packet(broadcast));
    dm.wait_response(string());
}

void import_storage(string const& storage_address,
                              manager& sm_server,
                              beltpp::ip_address const& connect_to_address)
{
    if (false == sm_server.storages.contains(storage_address))
    {
        beltpp::on_failure guard([&sm_server]
        {
            sm_server.files.discard();
            sm_server.storages.discard();
        });

        StringValue storage;
        storage.value = storage_address;
        sm_server.storages.insert(storage_address, storage);
    
        auto keys = sm_server.files.keys();
        for (auto const& key : keys)
        {
            FileInfo& file_info = sm_server.files.at(key);

            for (auto const& address : file_info.all_storages)
                if (address == storage_address)
                {
                    file_info.own_storages.push_back(storage_address);
                    
                    break;
                }
        }

        sm_server.files.save();
        sm_server.storages.save();

        guard.dismiss();

        sm_server.files.commit();
        sm_server.storages.commit();
    }
    else if (false == sm_server.m_str_pv_key.empty())
    {
        sm_daemon dm(sm_server);
        dm.open(connect_to_address);
        beltpp::finally finally_close([&dm] { dm.close(); });

        meshpp::private_key pv_key = meshpp::private_key(sm_server.m_str_pv_key);

        string progress_str;
        auto keys = sm_server.files.keys();
        size_t keys_count = keys.size();
        size_t index = 0;

        std::cout << std::endl << std::endl;
        for (auto const& key : keys)
        {
            ++index;

            if (index % 10 == 0)
            {
                std::cout << string(progress_str.length(), '\b');

                progress_str = std::to_string(index) + " files out of " + std::to_string(keys_count) + " are scaned...";
                std::cout << progress_str;

                std::this_thread::sleep_for(std::chrono::milliseconds(25));
            }

            FileInfo& file_info = sm_server.files.at(key);

            for (auto const& address : file_info.own_storages)
                if (address == storage_address)
                {
                    send_command(pv_key,
                                 file_info.uri,
                                 storage_address,
                                 file_info.channel_address,
                                 dm);

                    break;
                }
        }
    }
}

void manager::run()
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

                import_storage(msg.address, *this, connect_to_address);

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
            case StoragesRequest::rtt:
            {
                StoragesResponse response;
                for (auto const& storage : storages.keys())
                         response.storages.push_back(storage);

                rpc_socket->send(peerid, beltpp::packet(response));
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
        sm_daemon dm(*this);
        dm.open(connect_to_address);
        beltpp::finally finally_close([&dm]{ dm.close(); });

        dm.sync();
        dm.save();
        dm.commit();

        // send broadcast packet with storage management command
        if (false == m_str_pv_key.empty() && 
            storage_update_timer.expired())
        {
            storage_update_timer.update();
        
            auto manage_storages = storages.keys();
            auto storages_count = manage_storages.size();

            unordered_map<string, uint64_t> usage_map;
            uint64_t block_number = head_block_index.as_const()->value;
        
            for (auto index = block_number; index > 0 && index > block_number - 144; --index)
                for (auto const& item : m_file_usage_map[index])
                    usage_map[item.first] += item.second;
        
            for (auto it = m_file_usage_map.begin(); it != m_file_usage_map.end(); )
                if (it->first < block_number - 144)
                    it = m_file_usage_map.erase(it);
                else
                    ++it;
        
            // main solution
            //multimap<uint64_t, FileInfo> info_map;
            //for (auto it = usage_map.begin(); it != usage_map.end(); ++it)
            //{
            //    if (!files.contains(it->first))
            //        continue;
            //
            //    FileInfo const& file_info = files.at(it->first);
            //
            //    if(file_info.own_storages.size() < storages_count)
            //        info_map.insert({ it->second / file_info.all_storages.size(), file_info });
            //}

            // temp solution
            multimap<uint64_t, FileInfo> info_map;
            for (auto it = usage_map.begin(); it != usage_map.end(); ++it)
            {
                if (!files.contains(it->first))
                    continue;
            
                FileInfo const& file_info = files.at(it->first);
            
                if(file_info.own_storages.size() < storages_count)
                    info_map.insert({ it->second, file_info });
            }

            if (info_map.size())
            {
                meshpp::private_key pv_key = meshpp::private_key(m_str_pv_key);
                auto threshold = (info_map.begin()->first + info_map.rbegin()->first) / 2;

                auto it = info_map.rbegin();
                while (it->first > threshold && it != info_map.rend())
                {
                    auto temp_storages = manage_storages;
                    for (auto const& storage : it->second.own_storages)
                        temp_storages.erase(storage);

                    send_command(pv_key,
                                 it->second.uri,
                                 *temp_storages.begin(),
                                 it->second.channel_address,
                                 dm);

                    ++it;
                }
            }
        }
    }
}
