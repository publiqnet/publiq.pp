#include "documents.hpp"
#include "common.hpp"
#include "exception.hpp"
#include "types.hpp"
#include "message.tmpl.hpp"

#include <mesh.pp/fileutility.hpp>

using namespace BlockchainMessage;
namespace filesystem = boost::filesystem;

using std::string;
using std::vector;

namespace publiqpp
{
namespace detail
{
inline
beltpp::void_unique_ptr get_putl_types()
{
    beltpp::message_loader_utility utl;
    StorageTypes::detail::extension_helper(utl);

    auto ptr_utl =
        beltpp::new_void_unique_ptr<beltpp::message_loader_utility>(std::move(utl));

    return ptr_utl;
}

class documents_internals
{
public:
    documents_internals(filesystem::path const& path_documents,
                        filesystem::path const& path_storages)
        : m_files("file", path_documents, 10000, detail::get_putl())
        , m_units("unit", path_documents, 10000, detail::get_putl())
        , m_contents("content", path_documents, 10000, detail::get_putl())
        , m_storages("storages", path_storages, 10000, get_putl_types())
        , m_content_unit_sponsored_information("content_unit_info", path_documents, 10000, get_putl_types())
    {}

    meshpp::map_loader<File> m_files;
    meshpp::map_loader<ContentUnit> m_units;
    meshpp::map_loader<Content> m_contents;
    meshpp::map_loader<StorageTypes::FileUriHolders> m_storages;
    meshpp::map_loader<StorageTypes::ContentUnitSponsoredInformation> m_content_unit_sponsored_information;
};
}

documents::documents(filesystem::path const& path_documents,
                     filesystem::path const& path_storages)
    : m_pimpl(path_documents.empty() ? nullptr : new detail::documents_internals(path_documents, path_storages))
{}

documents::~documents() = default;

void documents::save()
{
    if (nullptr == m_pimpl)
        return;
    m_pimpl->m_files.save();
    m_pimpl->m_units.save();
    m_pimpl->m_contents.save();
    m_pimpl->m_storages.save();
    m_pimpl->m_content_unit_sponsored_information.save();
}

void documents::commit()
{
    if (nullptr == m_pimpl)
        return;
    m_pimpl->m_files.commit();
    m_pimpl->m_units.commit();
    m_pimpl->m_contents.commit();
    m_pimpl->m_storages.commit();
    m_pimpl->m_content_unit_sponsored_information.commit();
}

void documents::discard()
{
    if (nullptr == m_pimpl)
        return;
    m_pimpl->m_files.discard();
    m_pimpl->m_units.discard();
    m_pimpl->m_contents.discard();
    m_pimpl->m_storages.discard();
    m_pimpl->m_content_unit_sponsored_information.discard();
}

bool documents::exist_file(string const& uri) const
{
    if (uri.empty())
        return false;

    return m_pimpl->m_files.contains(uri);
}

bool documents::insert_file(File const& file)
{
    if (m_pimpl->m_files.contains(file.uri))
        return false;

    m_pimpl->m_files.insert(file.uri, file);

    return true;
}

void documents::remove_file(string const& uri)
{
    m_pimpl->m_files.erase(uri);
}

BlockchainMessage::File const& documents::get_file(std::string const& uri) const
{
    return m_pimpl->m_files.as_const().at(uri);
}

void documents::get_file_uris(vector<string>& file_uris) const
{
    file_uris.clear();

    for (auto it : m_pimpl->m_files.as_const().keys())
        file_uris.push_back(it);
}

bool documents::exist_unit(string const& uri) const
{
    if (uri.empty())
        return false;

    return m_pimpl->m_units.contains(uri);
}

bool documents::insert_unit(ContentUnit const& unit)
{
    if (m_pimpl->m_units.contains(unit.uri))
        return false;

    m_pimpl->m_units.insert(unit.uri, unit);

    return true;
}

void documents::remove_unit(string const& uri)
{
    m_pimpl->m_units.erase(uri);
}

BlockchainMessage::ContentUnit const& documents::get_unit(std::string const& uri) const
{
    return m_pimpl->m_units.as_const().at(uri);
}

void documents::get_unit_uris(vector<string>& unit_uris) const
{
    unit_uris.clear();

    for (auto it : m_pimpl->m_units.as_const().keys())
        unit_uris.push_back(it);
}

void documents::storage_update(std::string const& uri,
                               std::string const& address,
                               UpdateType status)
{
    if (UpdateType::store == status)
    {
        if (false == m_pimpl->m_storages.contains(uri))
        {
            StorageTypes::FileUriHolders holders;
            holders.addresses.insert(address);
            m_pimpl->m_storages.insert(uri, holders);
        }
        else
        {
            StorageTypes::FileUriHolders& holders = m_pimpl->m_storages.at(uri);
            holders.addresses.insert(address);
        }
    }
    else
    {
        if (m_pimpl->m_storages.contains(uri))
        {
            StorageTypes::FileUriHolders& holders = m_pimpl->m_storages.at(uri);
            holders.addresses.erase(address);

            if (holders.addresses.empty())
                m_pimpl->m_storages.erase(uri);
        }
    }
}

bool documents::storage_has_uri(std::string const& uri,
                                std::string const& address) const
{
    if (false == m_pimpl->m_storages.contains(uri))
        return false;

    StorageTypes::FileUriHolders const& holders = m_pimpl->m_storages.as_const().at(uri);
    return 0 != holders.addresses.count(address);
}

void documents::sponsor_content_unit_apply(BlockchainMessage::SponsorContentUnit const& spi)
{
    StorageTypes::SponsoredInformation si;
    si.amount = spi.amount;
    auto tp = std::chrono::system_clock::from_time_t(spi.start_time_point.tm);
    si.start_time_point.tm = std::chrono::system_clock::to_time_t(tp);
    si.end_time_point.tm = std::chrono::system_clock::to_time_t(tp + std::chrono::hours(spi.hours));

    if (m_pimpl->m_content_unit_sponsored_information.contains(spi.uri))
    {
        StorageTypes::ContentUnitSponsoredInformation& cusi =
                m_pimpl->m_content_unit_sponsored_information.at(spi.uri);
        cusi.sponsored_informations.push_back(si);
    }
    else
    {
        StorageTypes::ContentUnitSponsoredInformation cusi;
        cusi.uri = spi.uri;
        cusi.sponsored_informations.push_back(si);

        m_pimpl->m_content_unit_sponsored_information.insert(spi.uri, cusi);
    }
}
void documents::sponsor_content_unit_revert(BlockchainMessage::SponsorContentUnit const& spi)
{
    StorageTypes::SponsoredInformation si;
    si.amount = spi.amount;

    StorageTypes::ContentUnitSponsoredInformation& cusi =
            m_pimpl->m_content_unit_sponsored_information.at(spi.uri);

    if (cusi.sponsored_informations.empty() ||
        cusi.sponsored_informations.back().amount != si.amount)
    {
        assert(false);
        throw std::logic_error("cusi.sponsored_informations.back() != si");
    }

    cusi.sponsored_informations.resize(cusi.sponsored_informations.size() - 1);

    if (cusi.sponsored_informations.empty())
        m_pimpl->m_content_unit_sponsored_information.erase(spi.uri);
}
}

