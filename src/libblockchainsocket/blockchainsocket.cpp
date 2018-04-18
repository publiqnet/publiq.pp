#include "blockchainsocket.hpp"
#include "blockchainstate.hpp"
#include "message.hpp"

#include <belt.pp/packet.hpp>
#include <belt.pp/utility.hpp>

#include <mesh.pp/p2psocket.hpp>

#include <exception>
#include <string>
#include <memory>
#include <chrono>

using namespace BlockchainMessage;

using beltpp::ip_address;
using beltpp::ip_destination;
using beltpp::isocket;
using peer_id = isocket::peer_id;

namespace chrono = std::chrono;
using chrono::system_clock;
using chrono::steady_clock;
using std::string;
using std::vector;
using std::unique_ptr;

namespace publiqpp
{

using sf = meshpp::p2psocket_family_t<
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
    &TimerOut::saver
>;

namespace detail
{
class blockchainsocket_internals
{
public:
    blockchainsocket_internals(ip_address const& bind_to_address,
                               std::vector<ip_address> const& connect_to_addresses,
                               size_t rtt_error,
                               size_t rtt_join,
                               size_t rtt_drop,
                               size_t rtt_timer_out,
                               detail::fptr_creator fcreator_error,
                               detail::fptr_creator fcreator_join,
                               detail::fptr_creator fcreator_drop,
                               detail::fptr_creator fcreator_timer_out,
                               detail::fptr_saver fsaver_error,
                               detail::fptr_saver fsaver_join,
                               detail::fptr_saver fsaver_drop,
                               detail::fptr_saver fsaver_timer_out,
                               beltpp::void_unique_ptr&& putl,
                               beltpp::ilog* _plogger)
        : m_putl(putl.get())
        , m_ptr_socket(new meshpp::p2psocket(
            meshpp::getp2psocket<sf>(bind_to_address,
                                     connect_to_addresses,
                                     std::move(putl),
                                     _plogger)
                                          ))
        , m_ptr_state(publiqpp::getblockchainstate())
        , plogger(_plogger)
        , m_rtt_error(rtt_error)
        , m_rtt_join(rtt_join)
        , m_rtt_drop(rtt_drop)
        , m_rtt_timer_out(rtt_timer_out)
        , m_fcreator_error(fcreator_error)
        , m_fcreator_join(fcreator_join)
        , m_fcreator_drop(fcreator_drop)
        , m_fcreator_timer_out(fcreator_timer_out)
        , m_fsaver_error(fsaver_error)
        , m_fsaver_join(fsaver_join)
        , m_fsaver_drop(fsaver_drop)
        , m_fsaver_timer_out(fsaver_timer_out)
    {
        beltpp::message_loader_utility* _putl = static_cast<beltpp::message_loader_utility*>(m_putl);
        BlockchainMessage::detail::extension_helper(*_putl);
    }

    void write(string const& value)
    {
        if (plogger)
            plogger->message_no_eol(value);
    }

    void writeln(string const& value)
    {
        if (plogger)
            plogger->message(value);
    }

    void* m_putl;
    unique_ptr<meshpp::p2psocket> m_ptr_socket;
    publiqpp::blockchainstate_ptr m_ptr_state;

    beltpp::ilog* plogger;

    size_t m_rtt_error;
    size_t m_rtt_join;
    size_t m_rtt_drop;
    size_t m_rtt_timer_out;
    detail::fptr_creator m_fcreator_error;
    detail::fptr_creator m_fcreator_join;
    detail::fptr_creator m_fcreator_drop;
    detail::fptr_creator m_fcreator_timer_out;
    detail::fptr_saver m_fsaver_error;
    detail::fptr_saver m_fsaver_join;
    detail::fptr_saver m_fsaver_drop;
    detail::fptr_saver m_fsaver_timer_out;
};
}

/*
 * blockchainsocket
 */
blockchainsocket::blockchainsocket(ip_address const& bind_to_address,
                                   std::vector<ip_address> const& connect_to_addresses,
                                   size_t _rtt_error,
                                   size_t _rtt_join,
                                   size_t _rtt_drop,
                                   size_t _rtt_timer_out,
                                   detail::fptr_creator _fcreator_error,
                                   detail::fptr_creator _fcreator_join,
                                   detail::fptr_creator _fcreator_drop,
                                   detail::fptr_creator _fcreator_timer_out,
                                   detail::fptr_saver _fsaver_error,
                                   detail::fptr_saver _fsaver_join,
                                   detail::fptr_saver _fsaver_drop,
                                   detail::fptr_saver _fsaver_timer_out,
                                   beltpp::void_unique_ptr&& putl,
                                   beltpp::ilog* plogger)
    : isocket()
    , m_pimpl(new detail::blockchainsocket_internals(bind_to_address,
                                                     connect_to_addresses,
                                                     _rtt_error,
                                                     _rtt_join,
                                                     _rtt_drop,
                                                     _rtt_timer_out,
                                                     _fcreator_error,
                                                     _fcreator_join,
                                                     _fcreator_drop,
                                                     _fcreator_timer_out,
                                                     _fsaver_error,
                                                     _fsaver_join,
                                                     _fsaver_drop,
                                                     _fsaver_timer_out,
                                                     std::move(putl),
                                                     plogger))
{

}

blockchainsocket::blockchainsocket(blockchainsocket&&) = default;

blockchainsocket::~blockchainsocket()
{

}

blockchainsocket::packets blockchainsocket::receive(blockchainsocket::peer_id& peer)
{
    packets return_packets;
    while (return_packets.empty())
    {
        peer_id current_peer;
        packets received_packets =
                m_pimpl->m_ptr_socket->receive(current_peer);

        for (auto& received_packet : received_packets)
        {
            switch (received_packet.type())
            {
            case Join::rtt:
            {
                peer = current_peer;
                packet packet_join;
                packet_join.set(m_pimpl->m_rtt_join,
                                m_pimpl->m_fcreator_join(),
                                m_pimpl->m_fsaver_join);
                return_packets.emplace_back(std::move(packet_join));
                break;
            }
            case Error::rtt:
            {
                peer = current_peer;
                packet packet_error;
                packet_error.set(m_pimpl->m_rtt_error,
                                 m_pimpl->m_fcreator_error(),
                                 m_pimpl->m_fsaver_error);
                return_packets.emplace_back(std::move(packet_error));
                break;
            }
            case Drop::rtt:
            {
                peer = current_peer;
                packet packet_drop;
                packet_drop.set(m_pimpl->m_rtt_drop,
                                m_pimpl->m_fcreator_drop(),
                                m_pimpl->m_fsaver_drop);
                return_packets.emplace_back(std::move(packet_drop));
                break;
            }
            case TimerOut::rtt:
            {
                packet packet_timer_out;
                packet_timer_out.set(m_pimpl->m_rtt_timer_out,
                                     m_pimpl->m_fcreator_timer_out(),
                                     m_pimpl->m_fsaver_timer_out);
                return_packets.emplace_back(std::move(packet_timer_out));
                break;
            }
            case Other::rtt:
            {
                peer = current_peer;
                Other pack;
                std::move(received_packet).get(pack);
                return_packets.emplace_back(std::move(pack.contents));
                break;
            }
            }
        }
    }

    return return_packets;
}

void blockchainsocket::send(peer_id const& peer,
                            packet&& pack)
{
    Other wrapper;
    wrapper.contents = std::move(pack);
    m_pimpl->m_ptr_socket->send(peer, std::move(wrapper));
}

void blockchainsocket::set_timer(std::chrono::steady_clock::duration const& period)
{
    m_pimpl->m_ptr_socket->set_timer(period);
}

string blockchainsocket::name() const
{
    return m_pimpl->m_ptr_socket->name();
}
}


