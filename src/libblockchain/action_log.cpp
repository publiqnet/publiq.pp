#include "action_log.hpp"
#include "common.hpp"

#include <mesh.pp/fileutility.hpp>
#include <mesh.pp/cryptoutility.hpp>

using namespace BlockchainMessage;
namespace filesystem = boost::filesystem;

using std::string;

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
    {}

    meshpp::vector_loader<LoggedTransaction> m_actions;

    bool m_enabled;
    uint64_t m_revert_index;
};
}

action_log::action_log(boost::filesystem::path const& fs_action_log, bool log_enabled)
    : m_pimpl(new detail::action_log_internals(fs_action_log, log_enabled))
{
    m_pimpl->m_revert_index = length() - 1;
}
action_log::~action_log()
{
}

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

size_t action_log::length() const
{
    return m_pimpl->m_actions.as_const().size();
}

void action_log::log_block(BlockchainMessage::SignedBlock const& signed_block)
{
    if (!m_pimpl->m_enabled)
        return;

    Block block;
    signed_block.block_details.get(block);
    string block_hash = meshpp::hash(signed_block.to_string());

    BlockInfo block_info;
    block_info.block_hash = block_hash;
    block_info.sign_time = block.header.sign_time;
    block_info.authority = signed_block.authority;

    for (auto it = block.signed_transactions.begin(); it != block.signed_transactions.end(); ++it)
    {
        TransactionInfo transaction_info;
        transaction_info.fee = it->transaction_details.fee;
        transaction_info.sign_time = it->transaction_details.creation;
        transaction_info.transaction_hash = meshpp::hash(it->to_string());
        BlockchainMessage::detail::assign_packet(transaction_info.action, it->transaction_details.action);

        block_info.transactions.push_back(transaction_info);
    }

    for (auto it = block.rewards.begin(); it != block.rewards.end(); ++it)
    {
        RewardInfo reward_info;
        reward_info.to = it->to;
        reward_info.amount = it->amount;

        block_info.rewards.push_back(reward_info);
    }

    insert(std::move(block_info));
}

void action_log::log_transaction(SignedTransaction const& signed_transaction)
{
    if (!m_pimpl->m_enabled)
        return;

    TransactionInfo transaction_info;
    transaction_info.fee = signed_transaction.transaction_details.fee;
    transaction_info.sign_time = signed_transaction.transaction_details.creation;
    transaction_info.transaction_hash = meshpp::hash(signed_transaction.to_string());
    BlockchainMessage::detail::assign_packet(transaction_info.action, signed_transaction.transaction_details.action);

    insert(std::move(transaction_info));
}

void action_log::at(size_t number, LoggedTransaction& action_info) const
{
    action_info = m_pimpl->m_actions.as_const().at(number);
}

void action_log::insert(beltpp::packet&& action)
{
    LoggedTransaction action_info;
    action_info.applied_reverted = true;    //  apply
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

        revert = (action_info.applied_reverted == false);

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
    action_revert_info.applied_reverted = false;   //  revert
    m_pimpl->m_actions.push_back(action_revert_info);

    m_pimpl->m_revert_index = index - 1;
}
}
