#include "action_log.hpp"

#include "data.hpp"
#include "message.hpp"

#include <mesh.pp/fileutility.hpp>

#include <string>

namespace filesystem = boost::filesystem;
using std::string;

namespace publiqpp
{

namespace detail
{
class action_log_internals
{
public:
    action_log_internals(filesystem::path const& path)
        : m_path(path)
        , m_length(path / "length.txt")
        , m_scanned(path / "scanned.txt")
    {

    }

    using number_loader = meshpp::file_loader<Data::Number, &Data::Number::string_loader, &Data::Number::string_saver>;
    using number_locked_loader = meshpp::file_locker<number_loader>;

    filesystem::path m_path;
    number_locked_loader m_length;
    number_locked_loader m_scanned;
};
}

action_log::action_log(boost::filesystem::path const& fs_action_log)
    : m_pimpl(new detail::action_log_internals(fs_action_log))
{

}
action_log::~action_log()
{

}

size_t action_log::length() const
{
    return m_pimpl->m_length->value;
}

size_t action_log::scanned() const
{
    return m_pimpl->m_scanned->value;
}

void action_log::insert(BlockchainMessage::Reward const& msg_reward)
{
    using reward_loader = meshpp::file_loader<BlockchainMessage::Reward,
                                                &BlockchainMessage::Reward::string_loader,
                                                &BlockchainMessage::Reward::string_saver>;

    string file_name(std::to_string(length()));

    reward_loader rw(m_pimpl->m_path / file_name);
    *rw = msg_reward;
    rw.save();

    m_pimpl->m_length->value++;
    m_pimpl->m_length.save();
}

void action_log::insert(BlockchainMessage::NewArticle const& msg_new_article)
{
    using new_article_loader = meshpp::file_loader<BlockchainMessage::NewArticle,
                                                    &BlockchainMessage::NewArticle::string_loader,
                                                    &BlockchainMessage::NewArticle::string_saver>;

    string file_name(std::to_string(length()));
    new_article_loader na(m_pimpl->m_path / file_name);
    *na = msg_new_article;
    na.save();

    m_pimpl->m_length->value++;
    m_pimpl->m_length.save();
}
}
