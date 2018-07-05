#include "state.hpp"
#include "blockchain.hpp"
#include "action_log.hpp"
#include "storage.hpp"
#include "message.hpp"

#include <belt.pp/isocket.hpp>

#include <cryptopp/sha.h>
#include <cryptopp/filters.h>
#include <cryptopp/base64.h>

#include <utility>
#include <unordered_map>
#include <cassert>

using std::unordered_set;
using std::unordered_map;
using std::vector;

std::string SHA256HashString(std::string aString){
    std::string digest;
    CryptoPP::SHA256 hash;

    CryptoPP::StringSource foo(aString, true,
    new CryptoPP::HashFilter(hash,
      new CryptoPP::Base64Encoder (
         new CryptoPP::StringSink(digest))));

    return digest;
}

namespace filesystem = boost::filesystem;
using std::unique_ptr;

namespace publiqpp
{
namespace detail
{
class packet_and_expiry
{
public:
    beltpp::packet packet;
    size_t expiry;
};

class state_internals
{
public:
    state_internals(filesystem::path const& fs_blockchain,
                    filesystem::path const& fs_action_log,
                    filesystem::path const& fs_storage)
        : m_blockchain(fs_blockchain)
        , m_action_log(fs_action_log)
        , m_storage(fs_storage)
    {}

    publiqpp::blockchain m_blockchain;
    publiqpp::action_log m_action_log;
    publiqpp::storage m_storage;

    unordered_set<beltpp::isocket::peer_id> m_p2p_peers;
    unordered_map<beltpp::isocket::peer_id, packet_and_expiry> m_stored_requests;
};
}

state::state(filesystem::path const& fs_blockchain,
             filesystem::path const& fs_action_log,
             filesystem::path const& fs_storage)
    : m_pimpl(new detail::state_internals(fs_blockchain,
                                          fs_action_log,
                                          fs_storage))
{
}

state::~state()
{
}

publiqpp::blockchain& state::blockchain()
{
    return m_pimpl->m_blockchain;
}

publiqpp::action_log& state::action_log()
{
    return m_pimpl->m_action_log;
}

publiqpp::storage& state::storage()
{
    return m_pimpl->m_storage;
}

unordered_set<beltpp::isocket::peer_id> const& state::peers() const
{
    return m_pimpl->m_p2p_peers;
}

void state::add_peer(beltpp::isocket::peer_id const& peerid)
{
    std::pair<unordered_set<beltpp::isocket::peer_id>::iterator, bool> result =
            m_pimpl->m_p2p_peers.insert(peerid);

    if (result.second == false)
        throw std::runtime_error("p2p peer already exists: " + peerid);
}

void state::remove_peer(beltpp::isocket::peer_id const& peerid)
{
    reset_stored_request(peerid);
    if (0 == m_pimpl->m_p2p_peers.erase(peerid))
        throw std::runtime_error("p2p peer not found to remove: " + peerid);
}

void state::find_stored_request(beltpp::isocket::peer_id const& peerid,
                                beltpp::packet& packet)
{
    auto it = m_pimpl->m_stored_requests.find(peerid);
    if (it != m_pimpl->m_stored_requests.end())
    {
        BlockchainMessage::detail::assign_packet(packet, it->second.packet);
    }
}

void state::reset_stored_request(beltpp::isocket::peer_id const& peerid)
{
    m_pimpl->m_stored_requests.erase(peerid);
}

void state::store_request(beltpp::isocket::peer_id const& peerid,
                          beltpp::packet const& packet)
{
    detail::packet_and_expiry pck;
    BlockchainMessage::detail::assign_packet(pck.packet, packet);
    pck.expiry = 2;
    auto res = m_pimpl->m_stored_requests.insert(std::make_pair(peerid, std::move(pck)));
    if (false == res.second)
        throw std::runtime_error("only one request is supported at a time");
}

vector<beltpp::isocket::peer_id> state::do_step()
{
    vector<beltpp::isocket::peer_id> result;

    for (auto& key_value : m_pimpl->m_stored_requests)
    {
        if (0 == key_value.second.expiry)
            result.push_back(key_value.first);

        --key_value.second.expiry;
    }
    return result;
}

}
