#pragma once

#include "coin.hpp"
#include "global.hpp"
#include "message.hpp"

#include <boost/filesystem/path.hpp>

#include <map>
#include <string>

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

    void log_block(BlockchainMessage::SignedBlock const& signed_block,
                   std::map<std::string, std::map<std::string, uint64_t>> const& unit_uri_view_counts,
                   std::map<std::string, coin> const& unit_sponsor_applied);
    void log_transaction(BlockchainMessage::SignedTransaction const& signed_transaction);
    void at(size_t number, BlockchainMessage::LoggedTransaction& action_info) const;
    void revert();
private:
    std::unique_ptr<detail::action_log_internals> m_pimpl;

    void insert(beltpp::packet&& action);
};

}
