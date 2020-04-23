#include <publiq.pp/network.hpp>
#include <publiq.pp/message.hpp>

#include <iostream>

using namespace std;

using namespace BlockchainMessage;
using namespace network_simulation_impl;

int main()
{
    //__debugbreak();

    cout << "__debugbreake() is not working" << endl;
    cout << "attach debuger if you want and enter a number!"<< endl;

    int a;
    cin >> a;

    network_simulation ns;
    event_handler_ns eh(ns);

    socket_ns socket1(eh);
    eh.add(socket1);

    socket_ns socket2(eh);
    eh.add(socket2);

    ip_address address1("127.0.0.1", 1111);
    ip_address address2("127.0.0.2", 2222, "127.0.0.1", 1111);

    socket1.listen(address1);
    auto peers = socket2.open(address2);

    std::unordered_set<event_item const*> sockets;

    peer_id peer;
    eh.wait(sockets);
    socket1.receive(peer);
    socket2.receive(peer);

    socket1.send(peers.front(), beltpp::packet(Done()));
    socket2.receive(peer);

    socket1.send(peers.front(), beltpp::packet(beltpp::stream_drop()));
    socket2.receive(peer);

    ip_address address3("127.0.0.3", 3333, "127.0.0.4", 4444);
    ip_address address4("127.0.0.4", 4444, "127.0.0.3", 3333);

    socket1.open(address3);
    socket2.open(address4);

    eh.wait(sockets);

    return 0;
}
