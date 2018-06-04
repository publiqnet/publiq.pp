#pragma once

#include "global.hpp"
#include <belt.pp/isocket.hpp>
#include <belt.pp/packet.hpp>
#include <belt.pp/message_global.hpp>
#include <belt.pp/ilog.hpp>

#include <boost/filesystem/path.hpp>

#include <memory>
#include <string>
#include <list>
#include <vector>
#include <chrono>

namespace publiqpp
{

namespace detail
{
    class blockchainsocket_internals;
    using fptr_creator = beltpp::void_unique_ptr(*)();
    using fptr_saver = beltpp::detail::fptr_saver;
    using fptr_creator_str = beltpp::void_unique_ptr(*)(std::string const&);
}

template <size_t _rtt_error,
          size_t _rtt_join,
          size_t _rtt_drop,
          detail::fptr_creator _fcreator_error,
          detail::fptr_creator _fcreator_join,
          detail::fptr_creator _fcreator_drop,
          detail::fptr_saver _fsaver_error,
          detail::fptr_saver _fsaver_join,
          detail::fptr_saver _fsaver_drop>
class blockchainsocket_family_t
{
public:
    static constexpr size_t rtt_error = _rtt_error;
    static constexpr size_t rtt_join = _rtt_join;
    static constexpr size_t rtt_drop = _rtt_drop;
    static constexpr detail::fptr_creator fcreator_error = _fcreator_error;
    static constexpr detail::fptr_creator fcreator_join = _fcreator_join;
    static constexpr detail::fptr_creator fcreator_drop = _fcreator_drop;
    static constexpr detail::fptr_saver fsaver_error = _fsaver_error;
    static constexpr detail::fptr_saver fsaver_join = _fsaver_join;
    static constexpr detail::fptr_saver fsaver_drop = _fsaver_drop;
};

class BLOCKCHAINSOCKETSHARED_EXPORT blockchainsocket : public beltpp::isocket
{
public:
    using peer_id = beltpp::isocket::peer_id;
    using peer_ids = std::list<peer_id>;
    using packet = beltpp::packet;
    using packets = beltpp::isocket::packets;

    blockchainsocket(beltpp::event_handler& eh,
                     beltpp::ip_address const& bind_to_address,
                     std::vector<beltpp::ip_address> const& connect_to_addresses,
                     boost::filesystem::path const& fs_blockchain,
                     size_t _rtt_error,
                     size_t _rtt_join,
                     size_t _rtt_drop,
                     detail::fptr_creator _fcreator_error,
                     detail::fptr_creator _fcreator_join,
                     detail::fptr_creator _fcreator_drop,
                     detail::fptr_saver _fsaver_error,
                     detail::fptr_saver _fsaver_join,
                     detail::fptr_saver _fsaver_drop,
                     beltpp::void_unique_ptr&& putl,
                     beltpp::ilog* plogger);
    blockchainsocket(blockchainsocket&& other);
    virtual ~blockchainsocket();

    void prepare_wait() override;

    packets receive(peer_id& peer) override;

    void send(peer_id const& peer,
              packet&& pack) override;

    void timer_action() override;

    std::string name() const;

    beltpp::ievent_item const& worker() const;

private:
    std::unique_ptr<detail::blockchainsocket_internals> m_pimpl;
};

template <typename T_blockchainsocket_family>
BLOCKCHAINSOCKETSHARED_EXPORT blockchainsocket getblockchainsocket(beltpp::event_handler& eh,
                                                                   beltpp::ip_address const& bind_to_address,
                                                                   std::vector<beltpp::ip_address> const& connect_to_addresses,
                                                                   beltpp::void_unique_ptr&& putl,
                                                                   boost::filesystem::path const& fs_blockchain,
                                                                   beltpp::ilog* plogger)
{
    return blockchainsocket(eh,
                            bind_to_address,
                            connect_to_addresses,
                            fs_blockchain,
                            T_blockchainsocket_family::rtt_error,
                            T_blockchainsocket_family::rtt_join,
                            T_blockchainsocket_family::rtt_drop,
                            T_blockchainsocket_family::fcreator_error,
                            T_blockchainsocket_family::fcreator_join,
                            T_blockchainsocket_family::fcreator_drop,
                            T_blockchainsocket_family::fsaver_error,
                            T_blockchainsocket_family::fsaver_join,
                            T_blockchainsocket_family::fsaver_drop,
                            std::move(putl),
                            plogger);
}

}

