#include "documents.hpp"
#include "common.hpp"
#include "exception.hpp"
#include "types.hpp"
#include "message.tmpl.hpp"

#include <mesh.pp/fileutility.hpp>

#include <chrono>
#include <algorithm>
#include <utility>

using namespace BlockchainMessage;
namespace filesystem = boost::filesystem;

using std::string;
using std::vector;
using std::pair;
using std::set;
using std::map;
namespace chrono = std::chrono;
using chrono::system_clock;
using time_point = system_clock::time_point;

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

namespace
{
void refresh_index(StorageTypes::ContentUnitSponsoredInformation& cusi)
{
    assert(false == cusi.time_points_used.empty());

    system_clock::time_point tp = system_clock::from_time_t(cusi.time_points_used.back().tm);

    vector<pair<size_t, system_clock::duration>> arr_index;
    for (auto index : cusi.index_si)
    {
        if (cusi.sponsored_informations.size() <= index)
            continue;

        auto const& item = cusi.sponsored_informations[index];

        auto item_start_tp = system_clock::from_time_t(item.start_time_point.tm);
        auto item_end_tp = system_clock::from_time_t(item.end_time_point.tm);

        assert(item.time_points_used_before <= cusi.time_points_used.size());
        if (item.time_points_used_before > cusi.time_points_used.size())
            throw std::logic_error("item.time_points_used_before > cusi.time_points_used.size()");

        if (tp >= item_end_tp &&
            cusi.time_points_used.size() > item.time_points_used_before)
            continue;

        system_clock::duration duration = chrono::seconds(0);
        if (item_start_tp > tp)
            duration = item_start_tp - tp;

        arr_index.push_back(std::make_pair(index, duration));
    }

    std::sort(arr_index.begin(), arr_index.end(),
              [](pair<size_t, system_clock::duration> const& lhs,
              pair<size_t, system_clock::duration> const& rhs)
    {
        return lhs.second < rhs.second;
    });

    cusi.index_si.clear();
    for (auto const& arr_index_item : arr_index)
        cusi.index_si.push_back(arr_index_item.first);
}
}

void documents::sponsor_content_unit_apply(BlockchainMessage::SponsorContentUnit const& spi)
{
    StorageTypes::SponsoredInformation si;
    si.amount = spi.amount;
    auto start_tp = chrono::time_point_cast<chrono::minutes>
                    (system_clock::from_time_t(spi.start_time_point.tm));
    auto end_tp = chrono::time_point_cast<chrono::minutes>
                  (start_tp + chrono::hours(spi.hours));

    if (end_tp <= start_tp)
        throw std::runtime_error("end_tp <= start_tp");

    si.start_time_point.tm = system_clock::to_time_t(start_tp);
    si.end_time_point.tm = system_clock::to_time_t(end_tp);

    si.sponsor_address = spi.sponsor_address;

    if (m_pimpl->m_content_unit_sponsored_information.contains(spi.uri))
    {
        StorageTypes::ContentUnitSponsoredInformation& cusi =
                m_pimpl->m_content_unit_sponsored_information.at(spi.uri);

        assert(false == cusi.sponsored_informations.empty());
        assert(false == cusi.time_points_used.empty());

        if (cusi.sponsored_informations.empty())
            throw std::logic_error("cusi.sponsored_informations.empty()");
        if (cusi.time_points_used.empty())
            throw std::logic_error("cusi.time_points_used.empty()");

        si.time_points_used_before = cusi.time_points_used.size();

        //  the index will be sorted below inside refresh index
        cusi.index_si.push_back(cusi.sponsored_informations.size());
        cusi.sponsored_informations.push_back(si);

        refresh_index(cusi);
    }
    else
    {
        StorageTypes::ContentUnitSponsoredInformation cusi;
        cusi.uri = spi.uri;

        cusi.time_points_used.push_back(si.start_time_point);

        si.time_points_used_before = cusi.time_points_used.size();  // 1

        //  the index will be sorted below inside refresh index
        cusi.index_si.push_back(cusi.sponsored_informations.size());
        cusi.sponsored_informations.push_back(si);

        refresh_index(cusi);

        m_pimpl->m_content_unit_sponsored_information.insert(spi.uri, cusi);
    }
}

void documents::sponsor_content_unit_revert(BlockchainMessage::SponsorContentUnit const& spi)
{
    StorageTypes::ContentUnitSponsoredInformation& cusi =
            m_pimpl->m_content_unit_sponsored_information.at(spi.uri);

    assert(false == cusi.sponsored_informations.empty());
    assert(cusi.sponsored_informations.back().amount == StorageTypes::Coin(spi.amount));
    assert(false == cusi.time_points_used.empty());
    assert(cusi.time_points_used.size() == cusi.sponsored_informations.back().time_points_used_before);

    if (cusi.sponsored_informations.empty() ||
        cusi.sponsored_informations.back().amount != StorageTypes::Coin(spi.amount))
        throw std::logic_error("cusi.sponsored_informations.back().amount != si");
    if (cusi.time_points_used.empty())
        throw std::logic_error("cusi.time_points_used.empty()");
    if (cusi.time_points_used.size() != cusi.sponsored_informations.back().time_points_used_before)
        throw std::logic_error("cusi.time_points_used.size() != cusi.sponsored_informations.back().time_points_used_before");

    cusi.sponsored_informations.resize(cusi.sponsored_informations.size() - 1);

    if (cusi.sponsored_informations.empty())
        m_pimpl->m_content_unit_sponsored_information.erase(spi.uri);
    else
        refresh_index(cusi);
}

map<string, coin> documents::sponsored_content_unit_set_used(string const& content_unit_uri,
                                                             time_point const& tp,
                                                             documents::e_sponsored_content_unit_set_used type)
{
    map<string, coin> result;

    if (m_pimpl->m_content_unit_sponsored_information.contains(content_unit_uri))
    {
        StorageTypes::ContentUnitSponsoredInformation& cusi =
                m_pimpl->m_content_unit_sponsored_information.at(content_unit_uri);

        assert(false == cusi.sponsored_informations.empty());
        assert(false == cusi.time_points_used.empty());

        if (cusi.sponsored_informations.empty())
            throw std::logic_error("cusi.sponsored_informations.empty()");
        if (cusi.time_points_used.empty())
            throw std::logic_error("cusi.time_points_used.empty()");

        auto end_tp = tp;
        end_tp = chrono::time_point_cast<chrono::minutes>(end_tp);
        auto start_tp = system_clock::from_time_t(cusi.time_points_used.back().tm);

        if ((sponsored_content_unit_set_used_apply == type && end_tp > start_tp) ||
            (sponsored_content_unit_set_used_revert == type && end_tp == start_tp))
        {
            if (sponsored_content_unit_set_used_revert == type)
            {
                cusi.time_points_used.resize(cusi.time_points_used.size() - 1);
                assert(false == cusi.time_points_used.empty());
                if (cusi.time_points_used.empty())
                    throw std::logic_error("cusi.time_points_used.empty()");
                start_tp = system_clock::from_time_t(cusi.time_points_used.back().tm);

                //  the index will be sorted below inside refresh index
                cusi.index_si.clear();
                for (size_t index = 0; index < cusi.sponsored_informations.size(); ++index)
                    cusi.index_si.push_back(index);

                refresh_index(cusi);
            }

            for (auto const& index_si_item : cusi.index_si)
            {
                auto const& item = cusi.sponsored_informations[index_si_item];

                auto item_start_tp = system_clock::from_time_t(item.start_time_point.tm);
                auto item_end_tp = system_clock::from_time_t(item.end_time_point.tm);

                if (item_start_tp >= end_tp)
                    break;

                coin whole = item.amount;
                auto whole_duration = item_end_tp - item_start_tp;

                auto part_start_tp = std::max(start_tp, item_start_tp);
                auto part_end_tp = std::min(end_tp, item_end_tp);

                if (item.time_points_used_before == cusi.time_points_used.size())
                    part_start_tp = item_start_tp;

                auto part_duration = part_end_tp - part_start_tp;

                coin part = whole / uint64_t(chrono::duration_cast<chrono::minutes>(whole_duration).count())
                                  * uint64_t(chrono::duration_cast<chrono::minutes>(part_duration).count());

                if (part_end_tp == item_end_tp)
                    part += whole % uint64_t(chrono::duration_cast<chrono::minutes>(whole_duration).count());

                coin& temp_result = result[item.sponsor_address];
                temp_result += part;
            };
        }

        if (sponsored_content_unit_set_used_apply == type)
        {
            StorageTypes::ctime ct;
            ct.tm = system_clock::to_time_t(end_tp);
            cusi.time_points_used.push_back(ct);

            refresh_index(cusi);
        }
    }

    return result;
}

set<std::string> documents::content_unit_uri_sponsor_expiring(time_point const& tp) const
{
    set<std::string> result;

    auto end_tp = tp;
    end_tp = chrono::time_point_cast<chrono::minutes>(end_tp);
    auto start_tp = end_tp - std::chrono::seconds(BLOCK_MINE_DELAY);

    auto content_unit_uris = m_pimpl->m_content_unit_sponsored_information.keys();
    //  scanning over all sponsored infos is not the best idea.
    //  will have to have an optimal index for this
    for (auto const& content_unit : content_unit_uris)
    {
        StorageTypes::ContentUnitSponsoredInformation const& cusi =
                m_pimpl->m_content_unit_sponsored_information.as_const().at(content_unit);

        set<string> sponsor_addresses;
        for (auto const& item : cusi.sponsored_informations)
        {
            //auto item_start_tp = system_clock::from_time_t(item.start_time_point.tm);
            auto item_end_tp = system_clock::from_time_t(item.end_time_point.tm);

            if (item_end_tp <= end_tp &&
                item_end_tp > start_tp)
            {
                result.insert(cusi.uri);
                break;
            }
        }
    }

    return result;
}

}

