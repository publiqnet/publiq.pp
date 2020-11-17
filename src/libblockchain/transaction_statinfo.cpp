#include "transaction_statinfo.hpp"
#include "common.hpp"
#include "node_internals.hpp"
#include "exception.hpp"

#include <mesh.pp/cryptoutility.hpp>

#include <algorithm>
#include <unordered_set>

using namespace BlockchainMessage;
using std::string;
using std::vector;
using std::unordered_set;

namespace publiqpp
{
vector<string> action_owners(ServiceStatistics const& service_statistics)
{
    return {service_statistics.server_address};
}
vector<string> action_participants(ServiceStatistics const& service_statistics)
{
    vector<string> result;
    for (auto const& item : service_statistics.file_items)
    {
        for (auto const& item2 : item.count_items)
        {
            result.push_back(item2.peer_address);
        }

        result.push_back(item.file_uri);
        result.push_back(item.unit_uri);
    }

    result.push_back(service_statistics.server_address);

    return result;
}

void action_validate(SignedTransaction const& signed_transaction,
                     ServiceStatistics const& service_statistics,
                     bool/* check_complete*/)
{
    //  this is checked in signed_transaction_validate
    assert(false == signed_transaction.authorizations.empty());

    if (signed_transaction.authorizations.size() != 1)
        throw authority_exception(signed_transaction.authorizations.back().address, string());

    meshpp::public_key pb_key_server_address(service_statistics.server_address);

    if (service_statistics.file_items.empty())
        throw wrong_data_exception("dummy statistics");

    for (auto const& file_item : service_statistics.file_items)
    {
        if (file_item.count_items.empty())
            throw wrong_data_exception("dummy statistics");

        for (auto const& count_item : file_item.count_items)
        {
            meshpp::public_key pb_key_peer_address(count_item.peer_address);
            if (count_item.count == 0)
                throw wrong_data_exception("dummy statistics");
        }
    }
}

bool action_is_complete(SignedTransaction const&/* signed_transaction*/,
                        ServiceStatistics const&/* service_statistics*/)
{
    return true;
}

bool action_can_apply(publiqpp::detail::node_internals const& impl,
                      SignedTransaction const& signed_transaction,
                      ServiceStatistics const& service_statistics,
                      state_layer layer)
{
    assert(signed_transaction.authorizations.size() == 1);

    auto signed_authority = signed_transaction.authorizations.front().address;
    if (false == impl.m_authority_manager.check_authority(service_statistics.server_address, signed_authority, ServiceStatistics::rtt))
        return false;

    NodeType node_type;
    if (false == impl.m_state.get_role(service_statistics.server_address, node_type) ||
        node_type == NodeType::blockchain)
        return false;

    auto balance = coin(impl.m_state.get_balance(service_statistics.server_address, state_layer::pool));

    if (node_type == NodeType::channel &&
        balance < CHANNEL_AMOUNT_THRESHOLD)
        return false;

    if (node_type == NodeType::storage &&
        balance < STORAGE_AMOUNT_THRESHOLD)
        return false;

    if (state_layer::chain == layer)
    {
        auto tp_end = system_clock::from_time_t(impl.m_blockchain.last_header().time_signed.tm);
        auto tp_start = tp_end - chrono::seconds(BLOCK_MINE_DELAY);

        if (service_statistics.start_time_point.tm != system_clock::to_time_t(tp_start) ||
            service_statistics.end_time_point.tm != system_clock::to_time_t(tp_end))
            return false;
    }
    else
    {
        string server_address;
        std::unordered_set<string> peers_set;
        std::unordered_set<string> addresses_set;

        for (auto const& file_item : service_statistics.file_items)
            for (auto const& count_item : file_item.count_items)
                // exclude myself from check list
                if (count_item.peer_address != impl.front_public_key().to_string())
                    peers_set.insert(count_item.peer_address);

        PublicAddressesInfo public_addresses = impl.m_nodeid_service.get_addresses();

        for (auto const& item : public_addresses.addresses_info)
        {
            if (item.seconds_since_checked > PUBLIC_ADDRESS_FRESH_THRESHHOLD_SECONDS)
                break;

            if (item.node_address == service_statistics.server_address)
                server_address = item.ip_address.local.address;
            else if (peers_set.count(item.node_address) > 0)
                addresses_set.insert(item.ip_address.local.address);
        }

        if (server_address.empty() &&
            service_statistics.server_address == impl.front_public_key().to_string())
            server_address = service_statistics.server_address;

        if (server_address.empty() ||
            peers_set.size() != addresses_set.size() ||
            addresses_set.count(server_address) > 0)
            return false;
    }

    unordered_set<string> file_uris, unit_uris;
    for (auto const& file_item : service_statistics.file_items)
    {
        file_uris.insert(file_item.file_uri);

        if (node_type == NodeType::storage)
        {
            if (false == file_item.unit_uri.empty())
                return false;
        }
        else if (node_type == NodeType::channel)
            unit_uris.insert(file_item.unit_uri);
    }

    auto check_file_uris = impl.m_documents.files_exist(file_uris);
    if (false == check_file_uris.first)
        return false;
    auto check_unit_uris = impl.m_documents.units_exist(unit_uris);
    if (false == check_unit_uris.first)
        return false;

    for (auto const& file_item : service_statistics.file_items)
    {
        if (node_type == NodeType::channel)
        {
            auto content_unit = impl.m_documents.get_unit(file_item.unit_uri);
            if (content_unit.file_uris.end() ==
                std::find(content_unit.file_uris.begin(),
                          content_unit.file_uris.end(),
                          file_item.file_uri))
                return false;
        }

        for (auto const& count_item : file_item.count_items)
        {
            NodeType item_node_type;
            if (false == impl.m_state.get_role(count_item.peer_address, item_node_type) ||
                item_node_type == NodeType::blockchain ||
                item_node_type == node_type)
                return false;
        }
    }

    return true;
}

void action_apply(publiqpp::detail::node_internals& impl,
                  SignedTransaction const& signed_transaction,
                  ServiceStatistics const& service_statistics,
                  state_layer layer)
{
    assert(signed_transaction.authorizations.size() == 1);

    auto signed_authority = signed_transaction.authorizations.front().address;
    if (false == impl.m_authority_manager.check_authority(service_statistics.server_address, signed_authority, ServiceStatistics::rtt))
        throw authority_exception(signed_authority, impl.m_authority_manager.get_authority(service_statistics.server_address, ServiceStatistics::rtt));

    NodeType node_type;
    if (false == impl.m_state.get_role(service_statistics.server_address, node_type) ||
        node_type == NodeType::blockchain)
        throw wrong_data_exception("process_stat_info -> wrong authority type : " + service_statistics.server_address);

    auto balance = coin(impl.m_state.get_balance(service_statistics.server_address, state_layer::pool));

    if (node_type == NodeType::channel &&
        balance < CHANNEL_AMOUNT_THRESHOLD)
        throw not_enough_balance_exception(service_statistics.server_address,
                                           balance,
                                           CHANNEL_AMOUNT_THRESHOLD);

    if (node_type == NodeType::storage &&
        balance < STORAGE_AMOUNT_THRESHOLD)
        throw not_enough_balance_exception(service_statistics.server_address,
                                           balance,
                                           STORAGE_AMOUNT_THRESHOLD);

    if (state_layer::chain == layer)
    {
        auto tp_end = system_clock::from_time_t(impl.m_blockchain.last_header().time_signed.tm);
        auto tp_start = tp_end - chrono::seconds(BLOCK_MINE_DELAY);

        if (service_statistics.start_time_point.tm != system_clock::to_time_t(tp_start) ||
            service_statistics.end_time_point.tm != system_clock::to_time_t(tp_end))
            throw wrong_data_exception("service statistics time range is incorrect");
    }
    else
    {
        string server_address;
        std::unordered_set<string> peers_set;
        std::unordered_set<string> addresses_set;

        for (auto const& file_item : service_statistics.file_items)
            for (auto const& count_item : file_item.count_items)
                // exclude myself from check list
                if (count_item.peer_address != impl.front_public_key().to_string())
                    peers_set.insert(count_item.peer_address);

        PublicAddressesInfo public_addresses = impl.m_nodeid_service.get_addresses();

        for (auto const& item : public_addresses.addresses_info)
        {
            if (item.seconds_since_checked > PUBLIC_ADDRESS_FRESH_THRESHHOLD_SECONDS)
                break;

            if (item.node_address == service_statistics.server_address)
                server_address = item.ip_address.local.address;
            else if (peers_set.count(item.node_address) > 0)
                addresses_set.insert(item.ip_address.local.address);
        }

        if (server_address.empty() &&
            service_statistics.server_address == impl.front_public_key().to_string())
            server_address = service_statistics.server_address;

        if (server_address.empty())
            throw wrong_data_exception("service statistics creator address is not verified");
        else if(peers_set.size() != addresses_set.size())
            throw wrong_data_exception("service statistics some channels and storages are not verified");
        else if(addresses_set.count(server_address) > 0)
            throw wrong_data_exception("service statistics contains channel and storage with same address");
    }

    unordered_set<string> file_uris, unit_uris;
    for (auto const& file_item : service_statistics.file_items)
    {
        file_uris.insert(file_item.file_uri);

        if (node_type == NodeType::storage)
        {
            if (false == file_item.unit_uri.empty())
                throw uri_exception(file_item.unit_uri, uri_exception::invalid);
        }
        else if (node_type == NodeType::channel)
            unit_uris.insert(file_item.unit_uri);
    }

    auto check_file_uris = impl.m_documents.files_exist(file_uris);
    if (false == check_file_uris.first)
        throw uri_exception(check_file_uris.second, uri_exception::missing);
    auto check_unit_uris = impl.m_documents.units_exist(unit_uris);
    if (false == check_unit_uris.first)
        throw uri_exception(check_unit_uris.second, uri_exception::missing);

    for (auto const& file_item : service_statistics.file_items)
    {
        if (node_type == NodeType::channel)
        {
            auto content_unit = impl.m_documents.get_unit(file_item.unit_uri);
            if (content_unit.file_uris.end() ==
                std::find(content_unit.file_uris.begin(),
                          content_unit.file_uris.end(),
                          file_item.file_uri))
                throw uri_exception(file_item.unit_uri, uri_exception::invalid);
        }

        for (auto const& count_item : file_item.count_items)
        {
            NodeType item_node_type;
            if (false == impl.m_state.get_role(count_item.peer_address, item_node_type) ||
                item_node_type == NodeType::blockchain ||
                item_node_type == node_type)
                throw wrong_data_exception("wrong node type : " + count_item.peer_address);
        }
    }
}

void action_revert(publiqpp::detail::node_internals& impl,
                   SignedTransaction const& signed_transaction,
                   ServiceStatistics const& service_statistics,
                   state_layer/* layer*/)
{
    assert(signed_transaction.authorizations.size() == 1);

    auto signed_authority = signed_transaction.authorizations.front().address;
    if (false == impl.m_authority_manager.check_authority(service_statistics.server_address, signed_authority, ServiceStatistics::rtt))
        throw std::logic_error("false == impl.m_authority_manager.check_authority(service_statistics.server_address, signed_authority, ServiceStatistics::rtt)");
}
}
