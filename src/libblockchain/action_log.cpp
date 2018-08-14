#include "action_log.hpp"
#include "common.hpp"

#include "data.hpp"
#include "message.hpp"

#include <mesh.pp/fileutility.hpp>

#include <string>

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
    action_log_internals(filesystem::path const& path)
        :m_actions("actions", path, detail::get_putl())
    {}

    meshpp::vector_loader<LoggedTransaction> m_actions;
};
}

action_log::action_log(boost::filesystem::path const& fs_action_log)
    : m_pimpl(new detail::action_log_internals(fs_action_log))
{

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
}

size_t action_log::length() const
{
    return m_pimpl->m_actions.as_const().size();
}

void action_log::log(beltpp::packet&& action)
{
    if (action.type() != Transfer::rtt && action.type() != Reward::rtt)
        throw std::runtime_error("No logable actio type!");

    LoggedTransaction action_info;
    action_info.applied_reverted = true;    //  apply
    action_info.index = 0; // will be set automatically
    action_info.action = std::move(action);

    insert(action_info);
}

void action_log::insert(LoggedTransaction const& action_info)
{
    m_pimpl->m_actions.push_back(action_info);
}

void action_log::at(size_t number, LoggedTransaction& action_info) const
{
    if (number >= length())
        throw std::runtime_error("There is no action at index:" + std::to_string(number));

    action_info = m_pimpl->m_actions.as_const().at(number);
}

void action_log::revert()
{
    int revert_mark = 0;
    size_t index = length() - 1;
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
    insert(action_revert_info);
}
}
