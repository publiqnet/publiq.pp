#include "documents.hpp"
#include "common.hpp"
#include "exception.hpp"

#include <mesh.pp/fileutility.hpp>

using namespace BlockchainMessage;
namespace filesystem = boost::filesystem;

using std::string;
using std::vector;

namespace publiqpp
{
namespace detail
{

class documents_internals
{
public:
    documents_internals(filesystem::path const& path)
        : m_files("file", path, 10000, detail::get_putl())
        , m_units("unit", path, 10000, detail::get_putl())
        , m_contents("content", path, 10000, detail::get_putl())
    {}

    meshpp::map_loader<File> m_files;
    meshpp::map_loader<ContentUnit> m_units;
    meshpp::map_loader<Content> m_contents;
};
}

documents::documents(filesystem::path const& fs_state)
    : m_pimpl(new detail::documents_internals(fs_state))
{
}

documents::~documents() = default;

void documents::save()
{
    m_pimpl->m_files.save();
    m_pimpl->m_units.save();
    m_pimpl->m_contents.save();
}

void documents::commit()
{
    m_pimpl->m_files.commit();
    m_pimpl->m_units.commit();
    m_pimpl->m_contents.commit();
}

void documents::discard()
{
    m_pimpl->m_files.discard();
    m_pimpl->m_units.discard();
    m_pimpl->m_contents.discard();
}

bool documents::exist_file(string const& uri) const
{
    return m_pimpl->m_files.as_const().contains(uri);
}

bool documents::insert_file(File const& file)
{
    if (m_pimpl->m_files.as_const().contains(file.uri))
        return false;

    m_pimpl->m_files.insert(file.uri, file);

    return true;
}

void documents::remove_file(string const& uri)
{
    m_pimpl->m_files.erase(uri);
}

bool documents::exist_unit(string const& uri) const
{
    return m_pimpl->m_units.as_const().contains(uri);
}

bool documents::insert_unit(ContentUnit const& unit)
{
    if (m_pimpl->m_units.as_const().contains(unit.uri))
        return false;

    m_pimpl->m_units.insert(unit.uri, unit);

    return true;
}

void documents::remove_unit(string const& uri)
{
    m_pimpl->m_units.erase(uri);
}

//bool documents::exist_content(string const& uri) const
//{
//    return m_pimpl->m_contents.as_const().contains(uri);
//}
//
//bool documents::insert_content(Content const& file)
//{
//    if (m_pimpl->m_files.as_const().contains(file.uri))
//        return false;
//
//    m_pimpl->m_files.insert(file.uri, file);
//
//    return true;
//}
//
//void documents::remove_file(string const& uri)
//{
//    m_pimpl->m_files.erase(uri);
//}

}

