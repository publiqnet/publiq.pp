#pragma once

#include "global.hpp"

#include "blockchain.hpp"
#include "action_log.hpp"
#include "storage.hpp"

#include <belt.pp/isocket.hpp>

#include <boost/filesystem/path.hpp>

#include <memory>
#include <unordered_set>
#include <vector>

namespace publiqpp
{
namespace detail
{
class state_internals;
}
class state
{
public:
    state(boost::filesystem::path const& fs_blockchain,
          boost::filesystem::path const& fs_action_log,
          boost::filesystem::path const& fs_storage);
    ~state();

    publiqpp::blockchain& blockchain();
    publiqpp::action_log& action_log();
    publiqpp::storage& storage();

    std::unordered_set<beltpp::isocket::peer_id> const& peers() const;
    void add_peer(beltpp::isocket::peer_id const& peerid);
    void remove_peer(beltpp::isocket::peer_id const& peerid);
    void find_stored_request(beltpp::isocket::peer_id const& peerid,
                             beltpp::packet& packet);
    void reset_stored_request(beltpp::isocket::peer_id const& peerid);
    void store_request(beltpp::isocket::peer_id const& peerid,
                       beltpp::packet const& packet);
    std::vector<beltpp::isocket::peer_id> do_step();
private:
    std::unique_ptr<detail::state_internals> m_pimpl;
};
}
