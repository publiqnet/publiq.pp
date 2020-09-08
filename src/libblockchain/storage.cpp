#include "storage.hpp"
#include "common.hpp"

#include "types.hpp"
#include "message.hpp"

#include <mesh.pp/fileutility.hpp>
#include <mesh.pp/cryptoutility.hpp>

#include <belt.pp/utility.hpp>

#include <string>

namespace filesystem = boost::filesystem;
using std::pair;
using std::string;
using std::unordered_map;
using std::unordered_set;

namespace publiqpp
{

namespace detail
{
class storage_internals
{
public:
    storage_internals(filesystem::path const& path)
        : map("storage", path, 10000, detail::get_putl())
    {}

    meshpp::map_loader<BlockchainMessage::StorageFile> map;
};
}

storage::storage(boost::filesystem::path const& fs_storage)
    : m_pimpl(new detail::storage_internals(fs_storage))
{}
storage::~storage()
{}

bool storage::put(BlockchainMessage::StorageFile&& file, string& uri)
{
    bool code = false;
    uri = meshpp::hash(file.data);
    file.data = meshpp::to_base64(file.data, true);
    beltpp::on_failure guard([this]
    {
        m_pimpl->map.discard();
    });

    if (m_pimpl->map.insert(uri, file))
        code = true;

    m_pimpl->map.save();

    guard.dismiss();
    m_pimpl->map.commit();
    return code;
}

bool storage::get(string const& uri, BlockchainMessage::StorageFile& file)
{
    if (false == m_pimpl->map.contains(uri))
        return false;

    file = m_pimpl->map.as_const().at(uri);

    file.data = meshpp::from_base64(file.data);

    if (beltpp::chance_one_of(1000))
        m_pimpl->map.discard();

    return true;
}

bool storage::remove(string const& uri)
{
    if (false == m_pimpl->map.contains(uri))
        return false;

    beltpp::on_failure guard([this]
    {
        m_pimpl->map.discard();
    });
    m_pimpl->map.erase(uri);
    m_pimpl->map.save();

    guard.dismiss();
    m_pimpl->map.commit();

    return true;
}

unordered_set<string> storage::get_file_uris() const
{
    return m_pimpl->map.keys();
}

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

class storage_controller_internals
{
public:
    storage_controller_internals(filesystem::path const& path)
        : map("file_requests", path, 100, get_putl_types())
    {}

    meshpp::map_loader<StorageTypes::FileRequest> map;
    unordered_map<string, unordered_map<string, bool>> nodes_files_requesting;
};

}

storage_controller::storage_controller(boost::filesystem::path const& path_storage)
    : m_pimpl(path_storage.empty() ? nullptr : new detail::storage_controller_internals(path_storage))
{}
storage_controller::~storage_controller()
{}


void storage_controller::save()
{
    if (nullptr == m_pimpl)
        return;
    m_pimpl->map.save();
}

void storage_controller::commit() noexcept
{
    if (nullptr == m_pimpl)
        return;
    m_pimpl->map.commit();
}

void storage_controller::discard() noexcept
{
    if (nullptr == m_pimpl)
        return;
    m_pimpl->map.discard();
}

void storage_controller::clear()
{
    if (nullptr == m_pimpl)
        return;
    m_pimpl->map.clear();
}

void storage_controller::enqueue(string const& file_uri, 
                                 string const& node_address)
{
    if (nullptr == m_pimpl)
        return;

    StorageTypes::FileRequest fr;
    fr.file_uri = file_uri;
    fr.node_address = node_address;
    m_pimpl->map.insert(file_uri, fr);
}

void storage_controller::queue_redirect(std::string const& file_uri,
                                        std::string const& node_address,
                                        std::string const& order_token)
{
    if (false == m_pimpl->map.contains(file_uri))
        throw std::logic_error("storage_controller::queue_redirect: false == m_pimpl->map.contains(file_uri)");

    auto& file_request_item = m_pimpl->map.at(file_uri);
    file_request_item.node_address = node_address;
    file_request_item.order_token = order_token;
}

void storage_controller::pop(string const& file_uri, string const& node_address)
{
    if (nullptr == m_pimpl)
        return;

    auto it_node = m_pimpl->nodes_files_requesting.find(node_address);
    if (it_node != m_pimpl->nodes_files_requesting.end())
    {
        auto it_file = it_node->second.find(file_uri);
        if (it_file != it_node->second.end())
            throw std::logic_error("pop: it_file != it_channel->second.end()");
    }

    auto const& fr = m_pimpl->map.as_const().at(file_uri);
    if (fr.node_address == node_address)
    {
        beltpp::on_failure guard([this]{ discard(); });
        m_pimpl->map.erase(file_uri);
        save();
        guard.dismiss();
        commit();
    }
}

void storage_controller::initiate(string const& file_uri,
                                  string const& node_address,
                                  initiate_type e_initiate_type)
{
    if (nullptr == m_pimpl)
        return;

    auto it_node = m_pimpl->nodes_files_requesting.find(node_address);
    if (it_node == m_pimpl->nodes_files_requesting.end())
        throw std::logic_error("initiate: it_node == m_pimpl->nodes_files_requesting.end()");

    auto it_file = it_node->second.find(file_uri);
    if (it_file == it_node->second.end())
        throw std::logic_error("initiate: it_file == it_node->second.end()");

    if (check == e_initiate_type)
    {
        if (it_file->second)
            throw std::logic_error("initiate: false != it_file->second");

        it_file->second = true;
    }
    else
    {
        it_node->second.erase(it_file);
        if (it_node->second.empty())
            m_pimpl->nodes_files_requesting.erase(it_node);
    }
}

unordered_map<string, pair<string, string>> storage_controller::get_file_requests(unordered_set<string> const& resolved_nodes)
{
    unordered_map<string, pair<string, string>> file_to_node;
    if (nullptr == m_pimpl)
        return file_to_node;

    size_t count_all = 0;
    for (auto const& item : m_pimpl->nodes_files_requesting)
        count_all += item.second.size();
    if (count_all)
        return file_to_node;

    auto file_uris = m_pimpl->map.as_const().keys();

    unordered_set<string> unresolved_nodes;

    for (auto const& file_uri : file_uris)
    {
        auto const& file_request = m_pimpl->map.as_const().at(file_uri);

        if (0 == resolved_nodes.count(file_request.node_address))
        {
            if (10 == unresolved_nodes.size())
            {
                if (0 == unresolved_nodes.count(file_request.node_address))
                    m_pimpl->map.erase(file_uri);
            }
            else
                unresolved_nodes.insert(file_request.node_address);

            continue;
        }

        if (count_all == STORAGE_MAX_FILE_REQUESTS)
            continue;   //  might as well break, but will let to collect the unresolved channels

        /*if (STORAGE_MAX_FILE_REQUESTS == m_pimpl->channels_files_requesting.size() &&
            0 == m_pimpl->channels_files_requesting.count(file_request.channel_address))
            continue;*/

        auto insert_res = m_pimpl->nodes_files_requesting[file_request.node_address].insert({file_uri, false});

        if (insert_res.second)
        {
            ++count_all;
            file_to_node[file_uri] = { file_request.node_address, file_request.order_token };
        }
    }

    return file_to_node;
}

}
