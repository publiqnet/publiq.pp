#pragma once

#include "global.hpp"

#include "message.hpp"

#include <boost/filesystem/path.hpp>

#include <vector>
#include <memory>

namespace publiqpp
{

namespace detail
{
class action_log_internals;
}

class action_log
{
public:
    action_log(boost::filesystem::path const& fs_action_log);
    ~action_log();

    size_t length() const;
    size_t scanned() const;

    void insert(BlockchainMessage::Reward const& msg_reward);
    void insert(BlockchainMessage::NewArticle const& msg_new_article);
private:
    std::unique_ptr<detail::action_log_internals> m_pimpl;
};

}
