#include "node_synchronization.hpp"
#include "common.hpp"
#include "node_internals.hpp"

using namespace BlockchainMessage;

namespace publiqpp
{
node_synchronization::node_synchronization(detail::node_internals& impl)
    : pimpl(&impl)
{

}
SyncInfo node_synchronization::net_sync_info() const
{
    SyncInfo result;

    result.c_sum = 0;
    result.number = 0;

    for (auto const& item : sync_responses)
    {
        SyncInfo const& sync_info = item.second.sync_info;

        if (sync_info.number == own_sync_info().number &&
            sync_info.c_sum > result.c_sum)
        {
            result = sync_info;
        }
    }

    return result;
}

SyncInfo node_synchronization::own_sync_info() const
{
    SyncInfo result;
    Block const& block = pimpl->m_blockchain.at(pimpl->m_blockchain.length() - 1).block_details;
    result.number = block.header.block_number;
    result.c_sum = block.header.c_sum;

    // calculate delta for next block for the case if I will mine it
    if (pimpl->is_miner())
    {
        string prev_hash = meshpp::hash(block.to_string());
        uint64_t delta = pimpl->calc_delta(pimpl->m_pb_key.to_string(),
                                           pimpl->get_balance().whole,
                                           prev_hash,
                                           block.header.c_const);

        result.c_sum += delta;
        ++result.number;
    }

    return result;
}
}
