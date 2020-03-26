#include <publiq.pp/message.hpp>
#include <publiq.pp/message.tmpl.hpp>

#include <belt.pp/ievent.hpp>
#include <belt.pp/socket.hpp>

#include <boost/filesystem.hpp>

#include <iostream>
#include <chrono>
#include <thread>

using namespace BlockchainMessage;
using peer_id = beltpp::socket::peer_id;

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
            std::cout << "\t\t\t HINT" << std::endl;
            std::cout << "argument 1: end point address:port" << std::endl;
            std::cout << "argument 2: server address:port" << std::endl;
            std::cout << "argument 3: start_index for LoggedTransactionsRequest" << std::endl;
            return 0;
        }

    std::cout << "Process running ..." << std::endl;

    beltpp::ip_address address1;
    address1.from_string( argv[1] );
    if ( address1.remote.empty() )
    {
        address1.remote = address1.local;
        address1.local = beltpp::ip_destination();
    }
    beltpp::socket::peer_id peerid1;
    beltpp::event_handler_ptr eh1_ptr = beltpp::libsocket::construct_event_handler();
    beltpp::event_handler& eh1 = *eh1_ptr;
    beltpp::socket_ptr sk1_ptr = beltpp::libsocket::getsocket<sf>( eh1 );
    beltpp::socket& sk1 = *sk1_ptr;
    eh1.add( sk1 );
    peerid1 = Connect( address1, sk1, eh1 );
    beltpp::packet receive_package1;


    beltpp::ip_address address2;
    address2.from_string( argv[2] );
    if ( address2.remote.empty() )
    {
        address2.remote = address2.local;
        address2.local = beltpp::ip_destination();
    }
    beltpp::socket::peer_id peerid2;

    beltpp::event_handler_ptr eh2_ptr = beltpp::libsocket::construct_event_handler();
    beltpp::event_handler& eh2 = *eh2_ptr;
    beltpp::socket_ptr sk2_ptr = beltpp::libsocket::getsocket<sf>( eh2 );
    beltpp::socket& sk2 = *sk2_ptr;
    eh2.add( sk2 );
    peerid2 = Connect( address2, sk2, eh2 );
    beltpp::packet receive_package2;

    LoggedTransactionsRequest logged_transactions_request1;
    logged_transactions_request1.start_index = std::atoi(argv[3]);
    Send( beltpp::packet(logged_transactions_request1), receive_package1, sk1, peerid1, eh1 );
    LoggedTransactions logged_transactions1;
    receive_package1.get( logged_transactions1 );

    LoggedTransactionsRequest logged_transactions_request2;
    logged_transactions_request2.start_index = std::atoi(argv[3]);
    Send( beltpp::packet(logged_transactions_request2), receive_package2, sk2, peerid2, eh2 );
    LoggedTransactions logged_transactions2;
    receive_package2.get( logged_transactions2 );

    auto it1 = logged_transactions1.actions.begin();
    auto it2 = logged_transactions2.actions.begin();
    for ( ; it1 != logged_transactions1.actions.end() && it2 != logged_transactions2.actions.end(); it1++, it2++ )
    {
        if ( it1->logging_type != it2->logging_type || it1->action.to_string() != it2->action.to_string())
        {

            std::cout << "\t\t\t    Difference\n" << std::endl;
            std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~First~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << std::endl;
            std::cout << "{\"rtt\":" << it1->rtt << ",\"logging_type\":" << BlockchainMessage::to_string(it1->logging_type) << ",\"index\":" << it1->index << ",\"action\": " << it1->action.to_string() << "}" << std::endl;
            std::cout << "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~Second~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" <<std::endl;
            std::cout << "{\"rtt\":" << it2->rtt << ",\"logging_type\":" << BlockchainMessage::to_string(it1->logging_type) << ",\"index\":" << it2->index << ",\"action\": " << it2->action.to_string() << "}" << std::endl;
            std::cout << std::endl;
            return 0;
        }
    }

    if ( it1 != logged_transactions1.actions.end() || it2 != logged_transactions2.actions.end() )
    {
        std::cout << " Different action count " << std::endl;
        return 0;
    }

    std::cout << "Congrats! action logs are same" << std::endl;

    }
    catch ( std::exception const& e )
    {
        std::cout << "exception: " << e.what() << std::endl;
    }

    return 0;
}

void Send(beltpp::packet&& send_package,
          beltpp::packet& receive_package,
          beltpp::socket& sk,
          peer_id peerid,
          beltpp::event_handler& eh)
{
//   cout << endl << "Request -> " << send_package.to_string() << endl;
   sk.send(peerid, std::move(send_package));
   while (true)
   {
       beltpp::stream::packets packets;
       std::unordered_set<beltpp::event_item const*> set_items;

       if (beltpp::event_handler::wait_result::event & eh.wait(set_items))
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

       if (packet.type() == beltpp::stream_drop::rtt)
       {
           throw std::runtime_error("server disconnected");
       }
       else if (packet.type() == beltpp::socket_open_refused::rtt ||
                packet.type() == beltpp::socket_open_error::rtt ||
                packet.type() == beltpp::stream_join::rtt)
       {
           assert(false);
           throw std::runtime_error("open error or join received: impossible");
       }

       ::detail::assign_packet(receive_package, packet);

       break;
   }
//   cout << endl << "Response <- " << endl << receive_package.to_string() << endl;
}

peer_id Connect(beltpp::ip_address const& open_address,
                beltpp::socket& sk,
                beltpp::event_handler& eh)
{
    sk.open(open_address);

    peer_id peerid;
    beltpp::stream::packets packets;
    std::unordered_set<beltpp::event_item const*> set_items;

    while (true)
    {

        if (beltpp::event_handler::wait_result::event == eh.wait(set_items))
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

        if (packet.type() == beltpp::socket_open_refused::rtt)
        {
            beltpp::socket_open_refused msg;
            packet.get(msg);
            throw std::runtime_error(msg.reason);
        }
        else if (packet.type() == beltpp::socket_open_error::rtt)
        {
            beltpp::socket_open_error msg;
            packet.get(msg);
            throw std::runtime_error(msg.reason);
        }
        else if (packet.type() != beltpp::stream_join::rtt)
        {
            assert(false);
            throw std::runtime_error("unexpected response: " + std::to_string(packet.type()));
        }

        break;
    }

    return peerid;
}
