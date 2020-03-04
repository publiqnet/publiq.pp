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
        : m_black_box("black_box", path, 1000, 1, detail::get_putl())
    {
    }

    meshpp::vector_loader<BlackBox> m_black_box;
};
}

black_box::black_box(boost::filesystem::path const& fs_black_box)
    : m_pimpl(new detail::black_box_internals(fs_black_box))
{
}

black_box::~black_box() = default;

void black_box::save()
{
    m_pimpl->m_black_box.save();
}

void black_box::commit() noexcept
{
    m_pimpl->m_black_box.commit();
}

void black_box::discard() noexcept
{
    m_pimpl->m_black_box.discard();
}

void black_box::clear()
{
    m_pimpl->m_black_box.clear();
}

}
