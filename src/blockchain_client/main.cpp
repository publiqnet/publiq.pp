#include <publiq.pp/message.hpp>
#include <publiq.pp/message.tmpl.hpp>

#include <belt.pp/socket.hpp>
#include <belt.pp/event.hpp>

#include <boost/filesystem.hpp>

#include <mesh.pp/cryptoutility.hpp>

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
                beltpp::event_handler& eh);

void Send(beltpp::packet&& send_package,
          beltpp::packet& receive_package,
          beltpp::socket& sk,
          peer_id peerid,
          beltpp::event_handler& eh);

using sf = beltpp::socket_family_t<&message_list_load>;

int main(int argc, char** argv)
{
    try
    {
        if (argc < 2)
        {
            cout << "usage: blockchain_client address:port" << endl;
            return 0;
        }

        beltpp::ip_address address;
        address.from_string(argv[1]);
        if (address.remote.empty())
        {
            address.remote = address.local;
            address.local = beltpp::ip_destination();
        }

        size_t count = 10;

        if (argc > 2)
            count = std::atoi(argv[2]);

        uint64_t fee = 0;

        if (argc > 3)
            fee = std::atoi(argv[3]);

        //__debugbreak();
        beltpp::socket::peer_id peerid;
        beltpp::event_handler eh;

        beltpp::socket_ptr ptr_sk = beltpp::getsocket<sf>(eh);
        beltpp::socket& sk = *ptr_sk;
        eh.add(sk);

        peerid = Connect(address, sk, eh);
        cout << endl << peerid << endl;

        beltpp::packet receive_package;

        /*  StorageFile file;
        file.mime_type = "audio/mpeg";

        boost::filesystem::ifstream fl;
        fl.open("/Users/sona/Downloads/3.mp3", std::ios_base::binary);
        if (fl)
        {
        auto end = std::istreambuf_iterator<char>();
        auto begin = std::istreambuf_iterator<char>(fl);
        file.data.assign(begin, end);
        file.data.resize(10000);
        }

        Send(file, receive_package, sk, peerid, eh);*/


        KeyPairRequest key_pair_request;
        key_pair_request.index = 0;

        key_pair_request.master_key = "ARMEN";
        Send(beltpp::packet(key_pair_request), receive_package, sk, peerid, eh);

        KeyPair armen_key;
        receive_package.get(armen_key);

        key_pair_request.master_key = "TIGRAN";
        Send(beltpp::packet(key_pair_request), receive_package, sk, peerid, eh);

        KeyPair tigran_key;
        receive_package.get(tigran_key);

        Transfer transfer;
        transfer.from = armen_key.public_key;
        transfer.to = tigran_key.public_key;
        transfer.amount.fraction = 100;

        Transaction transaction;
        transaction.creation.tm = system_clock::to_time_t(system_clock::now());
        transaction.expiry.tm = system_clock::to_time_t(system_clock::now() + chrono::minutes(11));
        transaction.action = transfer;
        transaction.fee.fraction = fee;

        SignRequest sign_request;
        sign_request.private_key = armen_key.private_key;
        sign_request.package = transaction;

        Send(beltpp::packet(sign_request), receive_package, sk, peerid, eh);

        Signature signature;
        receive_package.get(signature);

        Authority authorization;
        authorization.address = armen_key.public_key;
        authorization.signature = signature.signature;

        SignedTransaction signed_transaction;
        signed_transaction.authorizations.push_back(authorization);
        signed_transaction.transaction_details = transaction;

        Broadcast broadcast;
        broadcast.package = signed_transaction;

        for (size_t i = 0; i < count; ++i)
        {
            if(i % 100 == 0)
                cout << std::to_string(i) << endl;

            Send(beltpp::packet(broadcast), receive_package, sk, peerid, eh);

            //std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

    }
    catch (std::exception const& e)
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
    //cout << endl << "Request -> "; // << send_package.to_string() << endl;

   sk.send(peerid, std::move(send_package));

   while (true)
   {
       beltpp::stream::packets packets;
       std::unordered_set<beltpp::ievent_item const*> set_items;

       if (beltpp::ievent_handler::wait_result::event & eh.wait(set_items))
           packets = sk.receive(peerid);

       if (peerid.empty())
           continue;

       if (packets.empty())
       {
           assert(false);
           throw std::runtime_error("no packets received from specified channel");
       }
       if (packets.size() > 1)
           throw std::runtime_error("more than one responses is not supported");

       auto const& packet = packets.front();

       if (packet.type() == beltpp::stream_drop::rtt)
       {
           throw std::runtime_error("server disconnected");
       }
       if (packet.type() == beltpp::socket_open_refused::rtt ||
           packet.type() == beltpp::socket_open_error::rtt ||
           packet.type() == beltpp::stream_join::rtt)
       {
           assert(false);
           throw std::runtime_error("open error or join received: impossible");
       }
       
       ::detail::assign_packet(receive_package, packet);
       
       break;
   }

   //cout << endl << "Response <- ";// << endl << receive_package.to_string() << endl;
}

peer_id Connect(beltpp::ip_address const& open_address,
                beltpp::socket& sk,
                beltpp::event_handler& eh)
{
    sk.open(open_address);

    peer_id peerid;
    beltpp::stream::packets packets;
    std::unordered_set<beltpp::ievent_item const*> set_items;
    while(true)
    {
        if (beltpp::ievent_handler::wait_result::event & eh.wait(set_items))
            packets = sk.receive(peerid);

        if (peerid.empty())
            continue;

        if (packets.empty())
        {
            assert(false);
            throw std::runtime_error("no packets received from specified channel");
        }
        if (packets.size() > 1)
            throw std::runtime_error("more than one responses is not supported");

        auto const& packet = packets.front();

        if (packet.type() == beltpp::socket_open_refused::rtt)
        {
            beltpp::socket_open_refused msg;
            packet.get(msg);
            throw std::runtime_error(msg.reason);
        }
        if (packet.type() == beltpp::socket_open_error::rtt)
        {
            beltpp::socket_open_error msg;
            packet.get(msg);
            throw std::runtime_error(msg.reason);
        }
        if (packet.type() != beltpp::stream_join::rtt)
        {
            assert(false);
            throw std::runtime_error("unexpected response: " + std::to_string(packet.type()));
        }
        
        break;
    }

    return peerid;
}
