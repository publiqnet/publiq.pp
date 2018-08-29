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

    void log(beltpp::packet&& action);
    void insert(BlockchainMessage::LoggedTransaction& action_info);
    void at(size_t number, BlockchainMessage::LoggedTransaction& action_info) const;
    void revert();
private:
    std::unique_ptr<detail::action_log_internals> m_pimpl;
};

}
