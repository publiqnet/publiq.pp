#include "../libblockchain/message.hpp"

#include <belt.pp/socket.hpp>

#include <boost/filesystem.hpp>

#include <iostream>
#include <chrono>
#include <thread>

using namespace BlockchainMessage;
using peer_id = beltpp::socket::peer_id;

using std::cout;
using std::endl;
namespace chrono = std::chrono;
using std::chrono::system_clock;

peer_id Connect(beltpp::ip_address const& open_address,
                beltpp::socket& sk,
                beltpp::event_handler& evH);

void Send(beltpp::packet&& send_package,
          beltpp::packet& receive_package,
          beltpp::socket& sk,
          peer_id peerid,
          beltpp::event_handler& eh);

using sf = beltpp::socket_family_t<&message_list_load>;

int main( int argc, char** argv )
{
    try
    {
        if ( argc < 3 )
        {
            cout << "\t\t\t HINT" << endl;
            cout << "argument 1: end point address:port" << endl;
            cout << "argument 2: server address:port" << endl;
            cout << "argument 3: start_index for LoggedTransactionsRequest" << endl;
            return 0;
        }

    beltpp::ip_address address1;
    address1.from_string( argv[1] );
    if ( address1.remote.empty() )
    {
        address1.remote = address1.local;
        address1.local = beltpp::ip_destination();
    }
    beltpp::socket::peer_id peerid1;
    beltpp::event_handler eh1;
    beltpp::socket sk1 = beltpp::getsocket<sf>( eh1 );
    eh1.add( sk1 );
    peerid1 = Connect( address1, sk1, eh1 );
    beltpp::packet receive_package1;

    LoggedTransactionsRequest logged_transactions_request1;
    logged_transactions_request1.start_index = std::atoll( argv[3] );
    Send( logged_transactions_request1, receive_package1, sk1, peerid1, eh1 );
    LoggedTransactions logged_transactions1;
    receive_package1.get( logged_transactions1 );
    auto it1 = logged_transactions1.actions.begin();
    for ( ; it1 != logged_transactions1.actions.end(); it1++ )
    {
        cout << it1->applied_reverted << endl;
        cout << it1->index << endl;
    }


    beltpp::ip_address address2;
    address2.from_string( argv[2] );
    if ( address2.remote.empty() )
    {
        address2.remote = address2.local;
        address2.local = beltpp::ip_destination();
    }
    beltpp::socket::peer_id peerid2;
    beltpp::event_handler eh2;
    beltpp::socket sk2 = beltpp::getsocket<sf>( eh2 );
    eh2.add( sk2 );
    peerid2 = Connect( address2, sk2, eh2 );
    beltpp::packet receive_package2;

    LoggedTransactionsRequest logged_transactions_request2;
    logged_transactions_request2.start_index = std::atoll( argv[3] );
    Send( logged_transactions_request2, receive_package2, sk2, peerid2, eh2 );
    LoggedTransactions logged_transactions2;
    receive_package2.get( logged_transactions2 );
    auto it2 = logged_transactions2.actions.begin();
    for ( ; it2 != logged_transactions2.actions.end(); it2++ )
    {
        cout << it2->applied_reverted << endl;
        cout << it2->index << endl;
    }


    std::this_thread::sleep_for(std::chrono::seconds(1));

    }
    catch ( std::exception const& e )
    {
        cout << "exception: " << e.what() << endl;
    }

    return 0;
}

void Send(beltpp::packet&& send_package,
          beltpp::packet& receive_package,
          beltpp::socket& sk,
          peer_id peerid,
          beltpp::event_handler& eh)
{
   cout << endl << endl << "Package sent -> "<< endl << send_package.to_string() << endl;

   sk.send(peerid, std::move(send_package));

   while (true)
   {
       beltpp::isocket::packets packets;
       std::unordered_set<beltpp::ievent_item const*> set_items;

       if (beltpp::ievent_handler::wait_result::event == eh.wait(set_items))
           packets = sk.receive(peerid);

       if (peerid.empty())
           continue;

       if (packets.empty())
       {
           assert(false);
           throw std::runtime_error("no packets received from specified channel");
       }
       else if (packets.size() > 1)
       {
           throw std::runtime_error("more than one responses is not supported");
       }

       auto const& packet = packets.front();

       if (packet.type() == beltpp::isocket_drop::rtt)
       {
           throw std::runtime_error("server disconnected");
       }
       else if (packet.type() == beltpp::isocket_open_refused::rtt ||
                packet.type() == beltpp::isocket_open_error::rtt ||
                packet.type() == beltpp::isocket_join::rtt)
       {
           assert(false);
           throw std::runtime_error("open error or join received: impossible");
       }

       ::detail::assign_packet(receive_package, packet);

       break;
   }

   cout << endl << "Package received -> " << endl << receive_package.to_string() << endl;
}

peer_id Connect(beltpp::ip_address const& open_address,
                beltpp::socket& sk,
                beltpp::event_handler& eh)
{
    sk.open(open_address);

    peer_id peerid;
    beltpp::isocket::packets packets;
    std::unordered_set<beltpp::ievent_item const*> set_items;

    while(true)
    {

        if (beltpp::ievent_handler::wait_result::event == eh.wait(set_items))
            packets = sk.receive(peerid);

        if (peerid.empty())
            continue;

        if (packets.empty())
        {
            assert(false);
            throw std::runtime_error("no packets received from specified channel");
        }
        else if (packets.size() > 1)
        {
            throw std::runtime_error("more than one responses is not supported");
        }

        auto const& packet = packets.front();

        if (packet.type() == beltpp::isocket_open_refused::rtt)
        {
            beltpp::isocket_open_refused msg;
            packet.get(msg);
            throw std::runtime_error(msg.reason);
        }
        else if (packet.type() == beltpp::isocket_open_error::rtt)
        {
            beltpp::isocket_open_error msg;
            packet.get(msg);
            throw std::runtime_error(msg.reason);
        }
        else if (packet.type() != beltpp::isocket_join::rtt)
        {
            assert(false);
            throw std::runtime_error("unexpected response: " + std::to_string(packet.type()));
        }

        break;
    }

    return peerid;
}
