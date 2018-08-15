#include "../libblockchain/message.hpp"

#include <belt.pp/socket.hpp>

#include <iostream>
#include <chrono>
#include <thread>
#include <boost/filesystem.hpp>
using namespace BlockchainMessage;
using peer_id = beltpp::socket::peer_id;

using std::cout;
using std::endl;
namespace chrono = std::chrono;
using std::chrono::system_clock;

peer_id connect(char** argv, beltpp::socket& sk, beltpp::event_handler& evH);

void Send(beltpp::packet& send_package,
          beltpp::packet& receive_package,
          beltpp::socket& sk,
          peer_id peerid,
          beltpp::event_handler& eh);

using sf = beltpp::socket_family_t<&message_list_load>;

int main(int argc, char** argv)
{
    //__debugbreak();
    beltpp::socket::peer_id peerid;
    beltpp::event_handler eh;

    beltpp::socket sk = beltpp::getsocket<sf>(eh);
    eh.add(sk);

    peerid = connect(argv, sk, eh);
    cout << endl << peerid << endl;

    beltpp::packet send_package;
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

    send_package.set(file);
    Send(send_package, receive_package, sk, peerid, eh);*/


    KeyPairRequest key_pair_request;
    key_pair_request.index = 0;

    key_pair_request.master_key = "ROB";
    send_package.set(key_pair_request);
    Send(send_package, receive_package, sk, peerid, eh);

    KeyPair rob_key;
    receive_package.get(rob_key);

    key_pair_request.master_key = "SERZ";
    send_package.set(key_pair_request);
    Send(send_package, receive_package, sk, peerid, eh);

    KeyPair serz_key;
    receive_package.get(serz_key);

    Reward reward;
    reward.amount.whole = 10000;
    reward.to = rob_key.public_key;

    LogTransaction log_transaction;
    log_transaction.action = reward;
    send_package.set(log_transaction);
    Send(send_package, receive_package, sk, peerid, eh);
    
    //return 0;

    for (auto i = 0; i < 10; ++i)
    {
        Transfer transfer;
        transfer.from = rob_key.public_key;
        transfer.to = serz_key.public_key;
        transfer.amount.whole = 10 + i;

        Transaction transaction;
        transaction.creation.tm = system_clock::to_time_t(system_clock::now());
        transaction.expiry.tm = system_clock::to_time_t(system_clock::now() + chrono::hours(24));
        transaction.action = transfer;

        SignRequest sign_request;
        sign_request.private_key = rob_key.private_key;
        sign_request.package = transaction;

        send_package.set(sign_request);
        Send(send_package, receive_package, sk, peerid, eh);

        Signature signature;
        receive_package.get(signature);

        SignedTransaction signed_transaction;
        signed_transaction.authority = rob_key.public_key;
        signed_transaction.signature = signature.signature;
        signed_transaction.transaction_details = transaction;

        Broadcast broadcast;
        broadcast.echoes = 2;
        broadcast.package = signed_transaction;

        send_package.set(broadcast);
        Send(send_package, receive_package, sk, peerid, eh);

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}

void Send(beltpp::packet& send_package,
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

       if (peerid.empty() || packets.empty())
           continue;
       
       if (packets.front().type() == beltpp::isocket_drop::rtt)
           cout << "Connection dropped!" << endl;
       
       ::detail::assign_packet(receive_package, packets.front());
       
       break;
   }

   cout << endl << "Package received -> " << endl << receive_package.to_string() << endl;
}

peer_id connect(char** argv, beltpp::socket& sk, beltpp::event_handler& eh)
{
    beltpp::ip_address address_item;
    address_item.from_string(argv[1]);
    beltpp::ip_address open_address("", 0, 
                                    address_item.local.address,
                                    address_item.local.port,
                                    beltpp::ip_address::e_type::ipv4);
    sk.open(open_address);

    peer_id channel_id;
    beltpp::isocket::packets pcs;
    std::unordered_set<beltpp::ievent_item const*> set_items;
    while(true)
    {
        if (beltpp::ievent_handler::wait_result::event == eh.wait(set_items))
            pcs = sk.receive(channel_id);

        if (channel_id.empty() && pcs.empty())
            continue;
        
        break;
    }

    return channel_id;
}
