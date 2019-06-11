#pragma once

#include "coin.hpp"
#include "message.hpp"

#include <boost/filesystem/path.hpp>

#include <vector>
#include <string>
#include <chrono>

namespace publiqpp
{
namespace detail
{
class documents_internals;
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

    void sponsor_content_unit_apply(BlockchainMessage::SponsorContentUnit const& spi);
    void sponsor_content_unit_revert(BlockchainMessage::SponsorContentUnit const& spi);

private:
    std::unique_ptr<detail::documents_internals> m_pimpl;
};
}
