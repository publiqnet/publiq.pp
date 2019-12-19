#include "action_log.hpp"
#include "common.hpp"
#include "message.tmpl.hpp"

#include <mesh.pp/fileutility.hpp>
#include <mesh.pp/cryptoutility.hpp>

using namespace BlockchainMessage;
namespace filesystem = boost::filesystem;

using std::string;
using std::map;

namespace publiqpp
{
namespace detail
{
class action_log_internals
{
public:
    action_log_internals(filesystem::path const& path, bool log_enabled)
        : m_actions("actions", path, 10000, 100, detail::get_putl())
        , m_enabled(log_enabled)
        , m_revert_index(m_actions.as_const().size() - 1)
    {}

    meshpp::vector_loader<LoggedTransaction> m_actions;

    bool m_enabled;
    uint64_t m_revert_index;
};
}

action_log::action_log(boost::filesystem::path const& fs_action_log, bool log_enabled)
    : m_pimpl(new detail::action_log_internals(fs_action_log, log_enabled))
{
}
action_log::~action_log() = default;

void action_log::save()
{
    m_pimpl->m_actions.save();
}

void action_log::commit()
{
    m_pimpl->m_actions.commit();
}

void action_log::discard()
{
    m_pimpl->m_actions.discard();
    m_pimpl->m_revert_index = length() - 1;
}

void action_log::clear()
{
    m_pimpl->m_actions.clear();
}

size_t action_log::length() const
{
    return m_pimpl->m_actions.as_const().size();
}

void action_log::log_block(BlockchainMessage::SignedBlock const& signed_block,
                           map<string, map<string, uint64_t>> const& unit_uri_view_counts,
                           map<string, coin> const& applied_sponsor_items)
{
    if (!m_pimpl->m_enabled)
        return;

    Block const& block = signed_block.block_details;
    string block_serialized = block.to_string();
    string block_hash = meshpp::hash(block_serialized);

    BlockLog block_log;
    block_log.block_hash = block_hash;
    block_log.block_number = block.header.block_number;
    block_log.time_signed = block.header.time_signed;
    block_log.authority = signed_block.authorization.address;
    block_log.block_size = block_serialized.size();

    for (auto const& item : block.signed_transactions)
    {
        string transaction_serialized = item.to_string();
        TransactionLog transaction_log;
        transaction_log.fee = item.transaction_details.fee;
        transaction_log.time_signed = item.transaction_details.creation;
        transaction_log.transaction_hash = meshpp::hash(transaction_serialized);
        transaction_log.transaction_size = transaction_serialized.size();
        BlockchainMessage::detail::assign_packet(transaction_log.action, item.transaction_details.action);

        block_log.transactions.push_back(transaction_log);
    }

    for (auto const& item : block.rewards)
    {
        RewardLog reward_log;
        reward_log.to = item.to;
        reward_log.amount = item.amount;
        reward_log.reward_type = item.reward_type;

        block_log.rewards.push_back(reward_log);
    }

    for (auto const& item : unit_uri_view_counts)
    {
        ContentUnitImpactLog impact_log;
        impact_log.content_unit_uri = item.first;

        impact_log.views_per_channel.reserve(item.second.size());
        for (auto const& item_per_channel : item.second)
        {
            ContentUnitImpactPerChannel views;
            views.channel_address = item_per_channel.first;
            views.view_count = item_per_channel.second;
            impact_log.views_per_channel.push_back(views);
        }

        block_log.unit_uri_impacts.push_back(impact_log);
    }

    for (auto const& item : applied_sponsor_items)
    {
        SponsorContentUnitApplied applied_log;
        item.second.to_Coin(applied_log.amount);
        applied_log.transaction_hash = item.first;

        block_log.applied_sponsor_items.push_back(applied_log);
    }

    insert(beltpp::packet(std::move(block_log)));
}

void action_log::log_transaction(SignedTransaction const& signed_transaction)
{
    if (!m_pimpl->m_enabled)
        return;

    string transaction_serialized = signed_transaction.to_string();

    TransactionLog transaction_log;
    transaction_log.fee = signed_transaction.transaction_details.fee;
    transaction_log.time_signed = signed_transaction.transaction_details.creation;
    transaction_log.transaction_hash = meshpp::hash(transaction_serialized);
    transaction_log.transaction_size = transaction_serialized.size();
    BlockchainMessage::detail::assign_packet(transaction_log.action, signed_transaction.transaction_details.action);

    insert(beltpp::packet(std::move(transaction_log)));
}

void action_log::at(size_t number, LoggedTransaction& action_info) const
{
    action_info = m_pimpl->m_actions.as_const().at(number);
}

void action_log::insert(beltpp::packet&& action)
{
    LoggedTransaction action_info;
    action_info.logging_type = LoggingType::apply;
    action_info.index = length();
    action_info.action = std::move(action);

    m_pimpl->m_actions.push_back(action_info);
    m_pimpl->m_revert_index = action_info.index;
}

void action_log::revert()
{
    if (!m_pimpl->m_enabled)
        return;

    int revert_mark = 0;
    size_t index = m_pimpl->m_revert_index;
    bool revert = true;

    while (revert)
    {
        LoggedTransaction action_info;
        at(index, action_info);

        revert = (action_info.logging_type == LoggingType::revert);

        if (revert)
            ++revert_mark;
        else
            --revert_mark;

        if (revert_mark >= 0)
        {
            if (index == 0)
                throw std::runtime_error("Nothing to revert!");

            --index;
            revert = true;
        }
    }

    // revert last valid action
    LoggedTransaction action_revert_info;
    at(index, action_revert_info);
    assert(action_revert_info.logging_type == LoggingType::apply);
    action_revert_info.logging_type = LoggingType::revert;
    m_pimpl->m_actions.push_back(action_revert_info);

    m_pimpl->m_revert_index = index - 1;
}
}
