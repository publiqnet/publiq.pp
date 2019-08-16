#pragma once

#include "coin.hpp"
#include "message.hpp"

#include <boost/filesystem/path.hpp>

#include <vector>
#include <string>
#include <chrono>
#include <map>
#include <utility>

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
    void commit();
    void discard();

    bool exist_file(std::string const& uri) const;
    bool insert_file(BlockchainMessage::File const& file);
    void remove_file(std::string const& uri);
    BlockchainMessage::File const& get_file(std::string const& uri) const;
    void get_file_uris(std::vector<std::string>&) const;

    bool exist_unit(std::string const& uri) const;
    bool insert_unit(BlockchainMessage::ContentUnit const& content_unit);
    void remove_unit(std::string const& uri);
    BlockchainMessage::ContentUnit const& get_unit(std::string const& uri) const;
    void get_unit_uris(std::vector<std::string>&) const;

    void storage_update(std::string const& uri, std::string const& address, BlockchainMessage::UpdateType status);
    bool storage_has_uri(std::string const& uri, std::string const& address) const;

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

    std::map<std::string, coin>
    sponsored_content_unit_set_used(std::string const& content_unit_uri,
                                    std::chrono::system_clock::time_point const& tp,
                                    e_sponsored_content_unit_set_used type,
                                    std::string const& transaction_hash_to_cancel,
                                    bool pretend);

    std::vector<std::pair<std::string, std::string>>
    content_unit_uri_sponsor_expiring(size_t block_number) const;

private:
    std::unique_ptr<detail::documents_internals> m_pimpl;
};
}
