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
        , m_start(path / "start.txt")
        , m_end(path / "end.txt")
    {

    }

    using number_loader = meshpp::file_loader<Data::Number, &Data::Number::string_loader, &Data::Number::string_saver>;
    using number_locked_loader = meshpp::file_locker<number_loader>;

    filesystem::path m_path;
    number_locked_loader m_start;
    number_locked_loader m_end;
};
}

action_log::action_log(boost::filesystem::path const& fs_action_log)
    : m_pimpl(new detail::action_log_internals(fs_action_log))
{

}
action_log::~action_log()
{

}

void action_log::insert(BlockchainMessage::Reward const& msg_reward)
{
    using reward_loader = meshpp::file_loader<BlockchainMessage::Reward,
                                                &BlockchainMessage::Reward::string_loader,
                                                &BlockchainMessage::Reward::string_saver>;

    string file_name(std::to_string(m_pimpl->m_end->value));

    reward_loader rw(m_pimpl->m_path / file_name);
    *rw = msg_reward;
    rw.save();

    m_pimpl->m_end->value++;
    m_pimpl->m_end.save();
}

void action_log::insert(BlockchainMessage::NewArticle const& msg_new_article)
{
    using new_article_loader = meshpp::file_loader<BlockchainMessage::NewArticle,
                                                    &BlockchainMessage::NewArticle::string_loader,
                                                    &BlockchainMessage::NewArticle::string_saver>;

    string file_name(std::to_string(m_pimpl->m_end->value));
    new_article_loader na(m_pimpl->m_path / file_name);
    *na = msg_new_article;
    na.save();

    m_pimpl->m_end->value++;
    m_pimpl->m_end.save();
}

bool action_log::pop(beltpp::packet& action)
{
    if (m_pimpl->m_end->value == m_pimpl->m_start->value)
        return false;

    string file_name(std::to_string(m_pimpl->m_start->value));

    auto path = m_pimpl->m_path / file_name;
    std::istream_iterator<char> end, begin;
    boost::filesystem::ifstream fl;
    meshpp::load_file(path, fl, begin, end);

    beltpp::detail::session_special_data ssd;
    ::beltpp::detail::pmsg_all pmsgall(size_t(-1),
                                       ::beltpp::void_unique_ptr(nullptr, [](void*){}),
                                       nullptr);
    if (fl && begin != end)
    {
        string file_all(begin, end);
        beltpp::iterator_wrapper<char const> it_begin(file_all.begin());
        beltpp::iterator_wrapper<char const> it_end(file_all.end());
        pmsgall = BlockchainMessage::message_list_load(it_begin, it_end, ssd, nullptr);
    }

    if (pmsgall.rtt == 0 ||
        pmsgall.pmsg == nullptr)
    {
        throw std::runtime_error("action_log::pop(): " + path.string());
    }
    else
    {
        action.set(pmsgall.rtt,
                   std::move(pmsgall.pmsg),
                   pmsgall.fsaver);

        m_pimpl->m_start->value++;
        m_pimpl->m_start.save();
    }

    return true;
}
}
