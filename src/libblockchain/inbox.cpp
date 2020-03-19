#include "inbox.hpp"
#include "common.hpp"

#include <belt.pp/utility.hpp>
#include <mesh.pp/fileutility.hpp>

using namespace BlockchainMessage;
namespace filesystem = boost::filesystem;

namespace publiqpp
{
namespace detail
{
class inbox_internals
{
public:
    inbox_internals(filesystem::path const& path)
        : m_inbox("items", path, 1000, 100, detail::get_putl())
    {
    }

    meshpp::vector_loader<Letter> m_inbox;
};
}

inbox::inbox(boost::filesystem::path const& fs_inbox)
    : m_pimpl(fs_inbox.empty() ? nullptr : new detail::inbox_internals(fs_inbox))
{
}

inbox::~inbox() = default;

void inbox::save()
{
    if (nullptr == m_pimpl)
        return;
    m_pimpl->m_inbox.save();
}

void inbox::commit() noexcept
{
    if (nullptr == m_pimpl)
        return;
    m_pimpl->m_inbox.commit();
}

void inbox::discard() noexcept
{
    if (nullptr == m_pimpl)
        return;
    m_pimpl->m_inbox.discard();
}

void inbox::clear()
{
    if (nullptr == m_pimpl)
        return;
    m_pimpl->m_inbox.clear();
}

size_t inbox::length() const
{
    if (nullptr == m_pimpl)
        return 0;
    return m_pimpl->m_inbox.as_const().size();
}

void inbox::insert(BlockchainMessage::Letter const& letter)
{
    if (m_pimpl)
        m_pimpl->m_inbox.push_back(letter);
}

BlockchainMessage::Letter const& inbox::at(size_t number) const
{
     return m_pimpl->m_inbox.as_const().at(number);
}

}
