#pragma once

#include "global.hpp"
#include "message.hpp"

#include <boost/filesystem/path.hpp>

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

    size_t length() const;
    void insert(BlockchainMessage::HeldBox const& held_box);
    BlockchainMessage::HeldBox const& at(size_t number) const;

private:
    std::unique_ptr<detail::black_box_internals> m_pimpl;
};

}
