#pragma once

#include "coin.hpp"
#include "message.hpp"

#include <boost/filesystem/path.hpp>

#include <vector>
#include <string>

namespace publiqpp
{
namespace detail
{
class documents_internals;
}
class documents
{
public:
    documents(boost::filesystem::path const& fs_state);
    ~documents();

    void save();
    void commit();
    void discard();

    bool exist_file(std::string const& uri) const;
    bool insert_file(BlockchainMessage::File const& file);
    void remove_file(std::string const& uri);
    BlockchainMessage::File const& get_file(std::string const& uri) const;

    bool exist_unit(std::string const& uri) const;
    bool insert_unit(BlockchainMessage::ContentUnit const& content_unit);
    void remove_unit(std::string const& uri);
    BlockchainMessage::ContentUnit const& get_unit(std::string const& uri) const;

private:
    std::unique_ptr<detail::documents_internals> m_pimpl;
};
}
