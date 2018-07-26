#include "../libblockchain/message.hpp"

#include <belt.pp/log.hpp>
#include <belt.pp/packet.hpp>
#include <belt.pp/socket.hpp>
#include <belt.pp/event.hpp>

#include <boost/program_options.hpp>

#include <memory>
#include <iostream>
#include <vector>
#include <sstream>
#include <chrono>
#include <memory>
#include <exception>


using namespace BlockchainMessage;
//namespace program_options = boost::program_options;

using std::unique_ptr;
using std::string;
using std::cout;
using std::endl;
using std::vector;
namespace chrono = std::chrono;
using chrono::steady_clock;
using std::runtime_error;

bool process_command_line(int argc, char** argv);

using namespace BlockchainMessage;


//  MSVS does not instansiate template function only because its address
//  is needed, so let's force it
template beltpp::void_unique_ptr beltpp::new_void_unique_ptr<Error>();
template beltpp::void_unique_ptr beltpp::new_void_unique_ptr<Join>();
template beltpp::void_unique_ptr beltpp::new_void_unique_ptr<Drop>();

using sf = beltpp::socket_family_t<
    Error::rtt,
    Join::rtt,
    Drop::rtt,
    &beltpp::new_void_unique_ptr<Error>,
    &beltpp::new_void_unique_ptr<Join>,
    &beltpp::new_void_unique_ptr<Drop>,
    &Error::saver,
    &Join::saver,
    &Drop::saver,
    &message_list_load
>;

int main(int argc, char** argv)
{
    const int listen_count = 10;

    beltpp::event_handler eh;
    beltpp::socket sk = beltpp::getsocket<sf>(eh);
    eh.add(sk);

    std::vector<beltpp::socket::peer_id> arr_channel_id;
    arr_channel_id.resize(100);

    for (size_t i = 0; i < 20000; ++i)
    {
        short index = short(i % arr_channel_id.size());

        beltpp::ip_address open_address("", 0, "127.0.0.1", 3550 + (i % listen_count), beltpp::ip_address::e_type::ipv4);
        sk.open(open_address);

        std::unordered_set<beltpp::ievent_item const*> set_items;
        while (true)
        {
            beltpp::socket::peer_id channel_id;

            beltpp::isocket::packets pcs;
            if (beltpp::ievent_handler::wait_result::event == eh.wait(set_items))
                pcs = sk.receive(channel_id);

            if (channel_id.empty())
                continue;

            for (auto const& pc : pcs)
            {
                if (pc.type() == Join::rtt)
                {
                    std::cout << i << std::endl;
                    if (i >= arr_channel_id.size())
                        sk.send(arr_channel_id[index], Drop());

                    arr_channel_id[index] = channel_id;
                }
            }
            break;
        }

        //std::cout << sk.dump() << std::endl;
    }


    if (false == process_command_line(argc, argv))
        return 1;

    return 0;
}

bool process_command_line(int argc, char** argv)
{

    cout << "The command line args count = " << argc << endl;

    for(int i = 1; i<argc; ++i)
        cout << argv[i] << endl;

    return true;
}
