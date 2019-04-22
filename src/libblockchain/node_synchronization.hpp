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
class headers_action_data
{
public:
    double reverts_required = 0;
    std::vector<BlockchainMessage::BlockHeaderExtended> headers;
};

class node_synchronization
{
public:
    node_synchronization(detail::node_internals& impl);
    detail::node_internals* pimpl;
    bool blockchain_sync_in_progress;
    std::unordered_map<beltpp::isocket::peer_id, BlockchainMessage::SyncResponse> sync_responses;
    std::unordered_map<beltpp::isocket::peer_id, headers_action_data> headers_actions_data;
    BlockchainMessage::BlockHeaderExtended net_sync_info() const;
    BlockchainMessage::BlockHeaderExtended own_sync_info() const;
};
}
