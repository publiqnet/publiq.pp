#pragma once

#include "global.hpp"
#include "message.hpp"

#include <boost/filesystem/path.hpp>

#include <vector>

namespace publiqpp
{

namespace detail
{
class black_box_internals;
}

class black_box
{
public:
    black_box(boost::filesystem::path const& fs_black_box);
    ~black_box();

    void save();
    void commit() noexcept;
    void discard() noexcept;
    void clear();

private:
    std::unique_ptr<detail::black_box_internals> m_pimpl;
};

}
