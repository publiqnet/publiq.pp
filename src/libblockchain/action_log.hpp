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

    void insert(beltpp::packet const& packet);
    bool at(size_t index, beltpp::packet& action) const;
private:
    std::unique_ptr<detail::action_log_internals> m_pimpl;
};

}
