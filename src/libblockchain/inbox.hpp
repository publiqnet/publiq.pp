#pragma once

#include "global.hpp"
#include "message.hpp"

#include <boost/filesystem/path.hpp>

namespace publiqpp
{

namespace detail
{
class inbox_internals;
}

class inbox
{
public:
    inbox(boost::filesystem::path const& fs_inbox);
    ~inbox();

    void save();
    void commit() noexcept;
    void discard() noexcept;
    void clear();

    size_t length() const;
    void insert(BlockchainMessage::Letter const& letter);
    BlockchainMessage::Letter const& at(size_t number) const;

private:
    std::unique_ptr<detail::inbox_internals> m_pimpl;
};

}
