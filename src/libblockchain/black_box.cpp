#include "black_box.hpp"
#include "common.hpp"

#include <belt.pp/utility.hpp>
#include <mesh.pp/fileutility.hpp>

using namespace BlockchainMessage;
namespace filesystem = boost::filesystem;

namespace publiqpp
{
namespace detail
{
class black_box_internals
{
public:
    black_box_internals(filesystem::path const& path)
        : m_black_box("black_box", path, 1000, 100, detail::get_putl())
    {
    }

    meshpp::vector_loader<HeldBox> m_black_box;
};
}

black_box::black_box(boost::filesystem::path const& fs_black_box)
    : m_pimpl(fs_black_box.empty() ? nullptr : new detail::black_box_internals(fs_black_box))
{
}

black_box::~black_box() = default;

void black_box::save()
{
    if (nullptr == m_pimpl)
        return;
    m_pimpl->m_black_box.save();
}

void black_box::commit() noexcept
{
    if (nullptr == m_pimpl)
        return;
    m_pimpl->m_black_box.commit();
}

void black_box::discard() noexcept
{
    if (nullptr == m_pimpl)
        return;
    m_pimpl->m_black_box.discard();
}

void black_box::clear()
{
    if (nullptr == m_pimpl)
        return;
    m_pimpl->m_black_box.clear();
}

size_t black_box::length() const
{
    return m_pimpl->m_black_box.as_const().size();
}

void black_box::insert(BlockchainMessage::HeldBox const& held_box)
{
    m_pimpl->m_black_box.push_back(held_box);
}

BlockchainMessage::HeldBox const& black_box::at(size_t number) const
{
     return m_pimpl->m_black_box.as_const().at(number);
}

}
