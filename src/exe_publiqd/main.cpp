#include "message.hpp"

#include <mesh.pp/p2psocket.hpp>

#include <boost/program_options.hpp>

#include <memory>
#include <iostream>

using namespace PubliqNodeMessage;

namespace program_options = boost::program_options;

using std::unique_ptr;
using std::string;
using std::cout;

/*using sf = beltpp::socket_family_t<
    Error::rtt,
    Join::rtt,
    Drop::rtt,
    TimerOut::rtt,
    &beltpp::new_void_unique_ptr<Error>,
    &beltpp::new_void_unique_ptr<Join>,
    &beltpp::new_void_unique_ptr<Drop>,
    &beltpp::new_void_unique_ptr<TimerOut>,
    &Error::saver,
    &Join::saver,
    &Drop::saver,
    &TimerOut::saver,
    &message_list_load
>;*/

int main(int argc, char** argv)
{
    program_options::options_description options_description;
    auto desc_init = options_description.add_options()
        ("help,h", "Print this help message and exit.")
        ("bind,b", program_options::value<unsigned short>(), "The local port for all connections")
        ("connect,c", program_options::value<string>(), "Remote node address with port");
    (void)(desc_init);

    program_options::variables_map options;

    program_options::store(
                program_options::parse_command_line(argc, argv, options_description),
                options);

    if (options.count("help"))
    {
        cout << options_description << "\n";
        return 0;
    }
    //unique_ptr<beltpp::socket> p_raw_sk(new beltpp::socket(beltpp::getsocket<sf>()));

    //meshpp::p2psocket sk(std::move(p_raw_sk));
    return 0;
}
