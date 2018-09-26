#pragma once

#include "global.hpp"

#include "message.hpp"

#include <boost/filesystem/path.hpp>

namespace publiqpp
{

namespace detail
{
class action_log_internals;
}

class action_log
{
public:
    action_log(boost::filesystem::path const& fs_action_log, bool log_enabled);
    ~action_log();

    void save();
    void commit();
    void discard();

    size_t length() const;

    void log_block(std::string const& authority, BlockchainMessage::ctime const& sign_time, std::string const& block_hash);
    void log_reward(BlockchainMessage::Reward const& reward, std::string const& block_hash);
    void log_transaction(BlockchainMessage::Transaction const& transaction, std::string const& transaction_hash);
    void at(size_t number, BlockchainMessage::LoggedTransaction& action_info) const;
    void revert();
private:
    std::unique_ptr<detail::action_log_internals> m_pimpl;

    void log(beltpp::packet&& action);
    void insert(BlockchainMessage::LoggedTransaction& action_info);
};

}
