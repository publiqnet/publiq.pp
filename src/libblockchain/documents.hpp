#pragma once

#include "global.hpp"
#include "coin.hpp"
#include "message.hpp"

#include <boost/filesystem/path.hpp>

#include <vector>
#include <unordered_set>
#include <string>
#include <map>
#include <utility>

namespace StorageTypes
{
    class SponsoredInformationHeaders;
    class SponsoredInformationHeader;
}
namespace publiqpp
{
namespace detail
{
class documents_internals;
class node_internals;
}
class documents
{
public:
    documents(boost::filesystem::path const& path_documents,
              boost::filesystem::path const& path_storages);
    ~documents();

    void save();
    void commit() noexcept;
    void discard() noexcept;
    void clear();

    std::pair<bool, std::string> files_exist(std::unordered_set<std::string> const& uris) const;
    bool file_exists(std::string const& uri) const;
    bool insert_file(BlockchainMessage::File const& file);
    void remove_file(std::string const& uri);
    BlockchainMessage::File const& get_file(std::string const& uri) const;

    std::pair<bool, std::string> units_exist(std::unordered_set<std::string> const& uris) const;
    bool unit_exists(std::string const& uri) const;
    bool insert_unit(BlockchainMessage::ContentUnit const& content_unit);
    void remove_unit(std::string const& uri);
    BlockchainMessage::ContentUnit const& get_unit(std::string const& uri) const;

    void storage_update(std::string const& uri, std::string const& address, BlockchainMessage::UpdateType status);
    bool storage_has_uri(std::string const& uri, std::string const& address) const;
    std::vector<std::string> get_file_storages(std::string const& uri) const;

public:

    void sponsor_content_unit_apply(publiqpp::detail::node_internals& impl,
                                    BlockchainMessage::SponsorContentUnit const& spi,
                                    std::string const& transaction_hash);
    void sponsor_content_unit_revert(publiqpp::detail::node_internals& impl,
                                     BlockchainMessage::SponsorContentUnit const& spi,
                                     std::string const& transaction_hash);

    enum e_sponsored_content_unit_set_used
    {
        sponsored_content_unit_set_used_apply,
        sponsored_content_unit_set_used_revert
    };

    std::map<std::string, std::map<std::string, coin>>
    sponsored_content_unit_set_used(publiqpp::detail::node_internals const& impl,
                                    std::string const& content_unit_uri,
                                    size_t block_number,
                                    e_sponsored_content_unit_set_used type,
                                    std::string const& transaction_hash_to_cancel,
                                    std::string const& manual_by_account,
                                    bool pretend);

    std::vector<std::pair<std::string, std::string>>
    content_unit_uri_sponsor_expiring(size_t block_number) const;

    StorageTypes::SponsoredInformationHeaders&
    expiration_entry_ref_by_block(uint64_t block_number);

    StorageTypes::SponsoredInformationHeader&
    expiration_entry_ref_by_block_by_hash(uint64_t block_number, std::string const& transaction_hash);

    StorageTypes::SponsoredInformationHeader&
    expiration_entry_ref_by_hash(std::string const& transaction_hash);

private:
    std::unique_ptr<detail::documents_internals> m_pimpl;
};
}
