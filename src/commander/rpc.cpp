#include "rpc.hpp"
#include "http.hpp"
#include "daemon_rpc.hpp"

#include <belt.pp/scope_helper.hpp>

#include <mesh.pp/settings.hpp>
#include <mesh.pp/cryptoutility.hpp>

#include <memory>
#include <chrono>
#include <unordered_set>

using std::unordered_set;
using std::unique_ptr;
namespace chrono = std::chrono;
using beltpp::packet;
using peer_id = beltpp::socket::peer_id;

using namespace CommanderMessage;

using sf = beltpp::socket_family_t<&commander::http::message_list_load<&CommanderMessage::message_list_load>>;

inline
beltpp::void_unique_ptr get_putl()
{
    beltpp::message_loader_utility utl;
    CommanderMessage::detail::extension_helper(utl);

    auto ptr_utl =
        beltpp::new_void_unique_ptr<beltpp::message_loader_utility>(std::move(utl));

    return ptr_utl;
}

rpc::rpc(beltpp::ip_address const& rpc_address,
         beltpp::ip_address const& connect_to_address)
    : eh()
    , rpc_socket(beltpp::getsocket<sf>(eh))
    , head_block_index(meshpp::data_file_path("head_block_index.txt"))
    , accounts("accounts", meshpp::data_directory_path("accounts"), 100, get_putl())
    , connect_to_address(connect_to_address)
{
    eh.set_timer(chrono::seconds(10));
    eh.add(rpc_socket);

    rpc_socket.listen(rpc_address);
}

void rpc::run()
{
    unordered_set<beltpp::ievent_item const*> wait_sockets;
    auto wait_result = eh.wait(wait_sockets);
    B_UNUSED(wait_sockets);

    if (wait_result == beltpp::event_handler::event)
    {
        beltpp::isocket::peer_id peerid;
        beltpp::socket::packets received_packets;

        received_packets = rpc_socket.receive(peerid);

        for (auto& received_packet : received_packets)
        {
        try
        {
            packet& ref_packet = received_packet;

            switch (ref_packet.type())
            {
            case AccountsRequest::rtt:
            {
                AccountsResponse msg;

                for (auto const& account : accounts.keys())
                {
                    msg.accounts.push_back(accounts.as_const().at(account));
                }

                rpc_socket.send(peerid, msg);
                break;
            }
            case ImportAccount::rtt:
            {
                ImportAccount msg;
                std::move(ref_packet).get(msg);

                Account account;
                account.address = msg.address;

                meshpp::public_key pb(account.address);

                if (false == accounts.contains(msg.address))
                {
                    daemon_rpc dm;
                    dm.open(connect_to_address);
                    beltpp::finally finally_close([&dm]{ dm.close(); });

                    unordered_set<string> set_accounts = {msg.address};

                    dm.sync(*this, set_accounts, true);
                }
                if (false == accounts.contains(msg.address))
                {
                    beltpp::on_failure guard([this](){accounts.discard();});
                    accounts.insert(msg.address, account);
                    accounts.save();

                    guard.dismiss();
                    accounts.commit();
                }

                rpc_socket.send(peerid, Done());
                break;
            }
            }
        }
        catch(std::exception const& ex)
        {
            Failed msg;
            msg.message = ex.what();
            rpc_socket.send(peerid, msg);
            throw;
        }
        catch(...)
        {
            Failed msg;
            msg.message = "unknown error";
            rpc_socket.send(peerid, msg);
            throw;
        }
        }
    }
    else
    {
        daemon_rpc dm;
        dm.open(connect_to_address);
        beltpp::finally finally_close([&dm]{ dm.close(); });

        dm.sync(*this, accounts.keys(), false);
    }
}
