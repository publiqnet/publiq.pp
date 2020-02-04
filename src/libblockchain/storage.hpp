#pragma once

#include "global.hpp"

#include "message.hpp"

#include <boost/filesystem/path.hpp>

#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace publiqpp
{

namespace detail
{
class storage_internals;
class storage_controller_internals;
}

class storage
{
public:
    storage(boost::filesystem::path const& fs_storage);
    ~storage();

    bool put(BlockchainMessage::StorageFile&& file, std::string& uri);
    bool get(std::string const& uri, BlockchainMessage::StorageFile& file);
    bool remove(std::string const& uri);
    std::unordered_set<std::string> get_file_uris() const;
private:
    std::unique_ptr<detail::storage_internals> m_pimpl;
};

class storage_controller
{
public:
    storage_controller(boost::filesystem::path const& fs_storage);
    ~storage_controller();

    void save();
    void commit() noexcept;
    void discard() noexcept;
    void clear();

    void enqueue(std::string const& file_uri, std::string const& channel_address);
    void pop(std::string const& file_uri, std::string const& channel_address);
    enum initiate_type {check, revert};
    void initiate(std::string const& file_uri,
                  std::string const& channel_address,
                  initiate_type e_initiate_type);
    std::unordered_map<std::string, std::string> get_file_requests(std::unordered_set<std::string> const& resolved_channels);
private:
    std::unique_ptr<detail::storage_controller_internals> m_pimpl;
};

}
