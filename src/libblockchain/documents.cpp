#include "documents.hpp"
#include "common.hpp"
#include "exception.hpp"
#include "types.hpp"
#include "node_internals.hpp"
#include "message.tmpl.hpp"

#include <mesh.pp/fileutility.hpp>

#include <chrono>
#include <algorithm>

using namespace BlockchainMessage;
namespace filesystem = boost::filesystem;

using std::string;
using std::vector;
using std::pair;
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
        , m_sponsored_informations_expiring("sponsored_info_expiring", path_documents, 10000, get_putl_types())
        , m_sponsored_informations_hash_to_block("sponsored_info_hash_to_block", path_documents, 10000, get_putl_types())
    {}

    meshpp::map_loader<File> m_files;
    meshpp::map_loader<ContentUnit> m_units;
    meshpp::map_loader<Content> m_contents;
    meshpp::map_loader<StorageTypes::FileUriHolders> m_storages;
    meshpp::map_loader<StorageTypes::ContentUnitSponsoredInformation> m_content_unit_sponsored_information;
    meshpp::map_loader<StorageTypes::SponsoredInformationHeaders> m_sponsored_informations_expiring;
    meshpp::map_loader<StorageTypes::TransactionHashToBlockNumber> m_sponsored_informations_hash_to_block;
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
    m_pimpl->m_sponsored_informations_expiring.save();
    m_pimpl->m_sponsored_informations_hash_to_block.save();
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
    m_pimpl->m_sponsored_informations_expiring.commit();
    m_pimpl->m_sponsored_informations_hash_to_block.commit();
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
    m_pimpl->m_sponsored_informations_expiring.discard();
    m_pimpl->m_sponsored_informations_hash_to_block.discard();
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

size_t get_expiring_block_number(publiqpp::detail::node_internals const& impl,
                                 chrono::system_clock::time_point const& item_end_tp)
{
    // calculate the block index which will take care of expiration
    auto genesis_tp =
            system_clock::from_time_t(impl.m_blockchain.header_at(0).time_signed.tm);

    size_t expiring_block_number = impl.m_blockchain.length();

    if (item_end_tp > genesis_tp)
    {
        uint64_t seconds = chrono::duration_cast<chrono::seconds>(item_end_tp - genesis_tp).count();
        size_t expected_block_number = seconds / BLOCK_MINE_DELAY;
        if (0 != seconds % BLOCK_MINE_DELAY)
            ++expected_block_number;

        if (expected_block_number > expiring_block_number)
            expiring_block_number = expected_block_number;
    }

    return expiring_block_number;
}
}

void documents::sponsor_content_unit_apply(publiqpp::detail::node_internals& impl,
                                           BlockchainMessage::SponsorContentUnit const& spi,
                                           string const& transaction_hash)
{
    StorageTypes::SponsoredInformation si;
    si.amount = spi.amount;
    auto item_start_tp = chrono::time_point_cast<chrono::seconds>
                         (system_clock::from_time_t(spi.start_time_point.tm));
    auto item_end_tp = chrono::time_point_cast<chrono::seconds>
                       (item_start_tp + chrono::hours(spi.hours));

    if (item_end_tp <= item_start_tp)
        throw std::runtime_error("item_end_tp <= item_start_tp");

    si.start_time_point.tm = system_clock::to_time_t(item_start_tp);
    si.end_time_point.tm = system_clock::to_time_t(item_end_tp);

    si.sponsor_address = spi.sponsor_address;
    si.transaction_hash = transaction_hash;
    si.cancelled = false;

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

    size_t expiring_block_number = get_expiring_block_number(impl, item_end_tp);

    StorageTypes::SponsoredInformationHeader expiring;
    expiring.uri = spi.uri;
    expiring.transaction_hash = si.transaction_hash;
    expiring.block_number = expiring_block_number;
    expiring.manually_cancelled = StorageTypes::Coin();

    if (false == m_pimpl->m_sponsored_informations_expiring.contains(std::to_string(expiring_block_number)))
    {
        StorageTypes::SponsoredInformationHeaders expirings;
        expirings.expirations[expiring.transaction_hash] = expiring;
        m_pimpl->m_sponsored_informations_expiring.insert(std::to_string(expiring_block_number),
                                                          expirings);
    }
    else
    {
        StorageTypes::SponsoredInformationHeaders& expirings =
                m_pimpl->m_sponsored_informations_expiring.at(std::to_string(expiring_block_number));

        expirings.expirations[expiring.transaction_hash] = expiring;
    }

    StorageTypes::TransactionHashToBlockNumber hash_to_block;
    hash_to_block.transaction_hash = si.transaction_hash;
    hash_to_block.block_number = expiring_block_number;

    m_pimpl->m_sponsored_informations_hash_to_block.insert(hash_to_block.transaction_hash,
                                                           hash_to_block);
}

void documents::sponsor_content_unit_revert(publiqpp::detail::node_internals& impl,
                                            BlockchainMessage::SponsorContentUnit const& spi,
                                            string const& transaction_hash)
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

    auto si = cusi.sponsored_informations.back();
    assert(si.transaction_hash == transaction_hash);
    if (si.transaction_hash != transaction_hash)
        throw std::logic_error("si.transaction_hash != transaction_hash");

    assert(si.cancelled != true);
    if (si.cancelled == true)
        throw std::logic_error("si.cancelled == true");

    auto item_end_tp = system_clock::from_time_t(si.end_time_point.tm);

    cusi.sponsored_informations.resize(cusi.sponsored_informations.size() - 1);

    if (cusi.sponsored_informations.empty())
        m_pimpl->m_content_unit_sponsored_information.erase(spi.uri);
    else
        refresh_index(cusi);

    size_t expiring_block_number = get_expiring_block_number(impl, item_end_tp);

    StorageTypes::SponsoredInformationHeaders& expirings = expiration_entry_ref(expiring_block_number);

    assert(expirings.expirations.count(si.transaction_hash));
    if (0 == expirings.expirations.count(si.transaction_hash))
        throw std::logic_error("0 == expirings.expirations.count(si.transaction_hash)");

    auto const& expiration_item = expirings.expirations[si.transaction_hash];
    assert(expiration_item.uri == spi.uri);
    if (expiration_item.uri != spi.uri)
        throw std::logic_error("expiration_item.uri != spi.uri");
    assert(expiration_item.block_number == expiring_block_number);
    if (expiration_item.block_number != expiring_block_number)
        throw std::logic_error("expiration_item.block_number != expiring_block_number");
    assert(expiration_item.transaction_hash == si.transaction_hash);
    if (expiration_item.transaction_hash != si.transaction_hash)
        throw std::logic_error("expiration_item.transaction_hash != si.transaction_hash");
    assert(expiration_item.manually_cancelled == StorageTypes::Coin());
    if (expiration_item.manually_cancelled != StorageTypes::Coin())
        throw std::logic_error("expiration_item.manually_cancelled != StorageTypes::Coin()");

    expirings.expirations.erase(si.transaction_hash);
    if (expirings.expirations.empty())
        m_pimpl->m_sponsored_informations_expiring.erase(std::to_string(expiring_block_number));

    StorageTypes::TransactionHashToBlockNumber hash_to_block;
    hash_to_block.transaction_hash = si.transaction_hash;
    hash_to_block.block_number = expiring_block_number;

    m_pimpl->m_sponsored_informations_hash_to_block.erase(hash_to_block.transaction_hash);
}

map<string, coin> documents::sponsored_content_unit_set_used(publiqpp::detail::node_internals const& impl,
                                                             string const& content_unit_uri,
                                                             size_t block_number,
                                                             documents::e_sponsored_content_unit_set_used type,
                                                             string const& transaction_hash_to_cancel,
                                                             string const& manual_by_account,
                                                             bool pretend)
{
    map<string, coin> result;

    if (transaction_hash_to_cancel.empty() ||
        sponsored_content_unit_set_used_revert == type)
        pretend = false;

    bool manual = (false == manual_by_account.empty());

    auto block_header = impl.m_blockchain.last_header();
    time_point tp = system_clock::from_time_t(block_header.time_signed.tm);
    if (block_number > block_header.block_number)
        tp += chrono::seconds(BLOCK_MINE_DELAY * (block_number - block_header.block_number));

    assert(block_number == block_header.block_number ||
           block_number == block_header.block_number + 1);
    if (block_number != block_header.block_number &&
        block_number != block_header.block_number + 1)
        throw std::logic_error("block_number range");

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
        end_tp = chrono::time_point_cast<chrono::seconds>(end_tp);
        auto start_tp = system_clock::from_time_t(cusi.time_points_used.back().tm);

        if ((sponsored_content_unit_set_used_apply == type && end_tp > start_tp) ||
            (sponsored_content_unit_set_used_revert == type &&
             (
                 (end_tp == start_tp && transaction_hash_to_cancel.empty()) ||
                 (end_tp > start_tp && false == transaction_hash_to_cancel.empty())
             )
            ))
        {
            if (sponsored_content_unit_set_used_revert == type &&
                transaction_hash_to_cancel.empty())
            {
                //  pretend mode cannot enter this path
                //
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
                auto& item = cusi.sponsored_informations[index_si_item];

                auto item_start_tp = system_clock::from_time_t(item.start_time_point.tm);
                auto item_end_tp = system_clock::from_time_t(item.end_time_point.tm);

                if (item_start_tp >= end_tp)
                    break;

                if (transaction_hash_to_cancel.empty() &&
                    item.cancelled)
                    continue;   //  regardless if doing apply or revert
                if (false == transaction_hash_to_cancel.empty() &&
                    item.transaction_hash != transaction_hash_to_cancel)
                    continue;   //  regardless if doing apply or revert - same

                coin whole = item.amount;
                auto whole_duration = item_end_tp - item_start_tp;

                auto part_start_tp = std::max(start_tp, item_start_tp);
                auto part_end_tp = std::min(end_tp, item_end_tp);

                if (false == transaction_hash_to_cancel.empty())
                    part_end_tp = item_end_tp;
                if (item.time_points_used_before == cusi.time_points_used.size())
                    part_start_tp = item_start_tp;

                auto part_duration = part_end_tp - part_start_tp;

                coin part = whole / uint64_t(chrono::duration_cast<chrono::seconds>(whole_duration).count())
                                  * uint64_t(chrono::duration_cast<chrono::seconds>(part_duration).count());

                if (part_end_tp == item_end_tp)
                    part += whole % uint64_t(chrono::duration_cast<chrono::seconds>(whole_duration).count());

                assert(part != coin());
                if (part == coin())
                    throw std::logic_error("part == coin()");

                coin& temp_result = result[item.sponsor_address];
                temp_result += part;

                if (false == manual_by_account.empty())
                {
                    if (item.sponsor_address != manual_by_account)
                    {
                        if (pretend)
                        {
                            result.clear();
                            return result;
                        }

                        throw authority_exception(manual_by_account,
                                                  item.sponsor_address);
                    }
                }

                if (false == transaction_hash_to_cancel.empty())
                {
                    size_t expiring_block_number = get_expiring_block_number(impl, item_end_tp);

                    auto& expiration_entry = expiration_entry_ref(expiring_block_number,
                                                                  item.transaction_hash);

                    if (sponsored_content_unit_set_used_apply == type)
                    {
                        if (item.cancelled)
                        {
                            if (pretend)
                            {
                                result.clear();
                                return result;
                            }
                            if (manual)
                                throw wrong_data_exception("already cancelled");
                            else
                                throw std::logic_error("already cancelled");
                        }

                        if (false == pretend)
                            item.cancelled = true;

                        assert(StorageTypes::Coin() == expiration_entry.manually_cancelled);
                        if (StorageTypes::Coin() != expiration_entry.manually_cancelled)
                            throw std::logic_error("StorageTypes::Coin() != expiration_entry.manually_cancelled");

                        if (manual && false == pretend)
                            part.to_Coin(expiration_entry.manually_cancelled);
                    }

                    if (sponsored_content_unit_set_used_revert == type)
                    {
                        //  here logic errors are ok even for pretend
                        assert(item.cancelled);
                        if (false == item.cancelled)
                            throw std::logic_error("false == item.cancelled");

                        if (false == pretend)
                            item.cancelled = false;

                        assert(manual == (expiration_entry.manually_cancelled != StorageTypes::Coin()));
                        if (manual != (expiration_entry.manually_cancelled != StorageTypes::Coin()))
                            throw std::logic_error("manual != (expiration_entry.manually_cancelled != StorageTypes::Coin())");

                        if (manual && false == pretend)
                            expiration_entry.manually_cancelled = StorageTypes::Coin();
                    }
                }
            };
        }

        if (sponsored_content_unit_set_used_apply == type &&
            transaction_hash_to_cancel.empty())
        {
            //  pretend mode cannot enter this path
            //
            StorageTypes::ctime ct;
            ct.tm = system_clock::to_time_t(end_tp);
            cusi.time_points_used.push_back(ct);

            refresh_index(cusi);
        }
    }

    return result;
}

vector<pair<string, string>> documents::content_unit_uri_sponsor_expiring(size_t block_number) const
{
    vector<pair<string, string>> result;

    if (m_pimpl->m_sponsored_informations_expiring.contains(std::to_string(block_number)))
    {
        auto const& expirings =
                m_pimpl->m_sponsored_informations_expiring.as_const().at(std::to_string(block_number));

        for (auto const& expirations_item : expirings.expirations)
        {
            if (StorageTypes::Coin() == expirations_item.second.manually_cancelled)
            {
                result.push_back(std::make_pair(expirations_item.second.uri,
                                                expirations_item.second.transaction_hash));
            }
        }
    }

    return result;
}

StorageTypes::SponsoredInformationHeaders&
documents::expiration_entry_ref(uint64_t block_number)
{
    assert(m_pimpl->m_sponsored_informations_expiring.contains(std::to_string(block_number)));
    if (false == m_pimpl->m_sponsored_informations_expiring.contains(std::to_string(block_number)))
        throw std::logic_error("false == m_pimpl->m_sponsored_informations_expiring.contains(std::to_string(block_number))");

    auto& expirings =
            m_pimpl->m_sponsored_informations_expiring.at(std::to_string(block_number));

    return expirings;
}

StorageTypes::SponsoredInformationHeader&
documents::expiration_entry_ref(uint64_t block_number, std::string const& transaction_hash)
{
    auto& expirings = expiration_entry_ref(block_number);

    assert(expirings.expirations.count(transaction_hash));
    if (0 == expirings.expirations.count(transaction_hash))
        throw std::logic_error("0 == expirings.expirations.count(transaction_hash)");

    auto& expiration_entry = expirings.expirations[transaction_hash];

    return expiration_entry;
}

StorageTypes::SponsoredInformationHeader&
documents::expiration_entry_ref(std::string const& transaction_hash)
{
    assert(m_pimpl->m_sponsored_informations_hash_to_block.contains(transaction_hash));
    if (false == m_pimpl->m_sponsored_informations_hash_to_block.contains(transaction_hash))
        throw std::logic_error("false == m_pimpl->m_sponsored_informations_hash_to_block.contains(transaction_hash)");

    auto const& hash_to_block = m_pimpl->m_sponsored_informations_hash_to_block.as_const().at(transaction_hash);

    auto& expirings = expiration_entry_ref(hash_to_block.block_number);

    assert(expirings.expirations.count(transaction_hash));
    if (0 == expirings.expirations.count(transaction_hash))
        throw std::logic_error("0 == expirings.expirations.count(transaction_hash)");

    auto& expiration_entry = expirings.expirations[transaction_hash];

    return expiration_entry;
}

}
