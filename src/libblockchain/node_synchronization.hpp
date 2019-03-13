#pragma once

#include "global.hpp"
#include "message.hpp"

#include <belt.pp/isocket.hpp>

#include <vector>
#include <utility>
#include <unordered_map>

namespace publiqpp
{
namespace detail
{
class node_internals;
}
}

namespace publiqpp
{
class node_synchronization
{
public:
    node_synchronization(detail::node_internals& impl);
    detail::node_internals* pimpl;
    std::unordered_map<beltpp::isocket::peer_id, BlockchainMessage::SyncResponse2> sync_responses;
    BlockchainMessage::SyncInfo net_sync_info() const;
    BlockchainMessage::SyncInfo own_sync_info() const;
    //
    BlockchainMessage::ctime sync_time;
    beltpp::isocket::peer_id sync_peerid;
    std::vector<BlockchainMessage::SignedBlock> sync_blocks;
    std::vector<BlockchainMessage::BlockHeader> sync_headers;
};
}
