#include "nodeid_service.hpp"
#include "common.hpp"
#include "sessions.hpp"
#include "message.tmpl.hpp"

#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/global_fun.hpp>
#include <boost/multi_index_container.hpp>

#include <utility>
#include <string>
#include <chrono>
#include <unordered_set>

using std::pair;
using std::vector;
using std::string;
using beltpp::ip_address;
namespace chrono = std::chrono;
using steady_clock = chrono::steady_clock;
using system_clock = chrono::system_clock;
using time_point = steady_clock::time_point;

namespace publiqpp
{

class nodeid_address_header
{
public:
    beltpp::ip_address address;
    string node_address;
    time_point checked_time_point;
};
class nodeid_address_unit
{
public:
    enum class verified_type {never, past, current};

    nodeid_address_unit();
    nodeid_address_unit(nodeid_address_unit&&);
    ~nodeid_address_unit();

    nodeid_address_unit& operator = (nodeid_address_unit&&);

    nodeid_address_header header;
    beltpp::ip_address ssl_address;
    std::unique_ptr<session_action_broadcast_address_info> ptr_action;
    verified_type verified = verified_type::never;
};

class nodeid_container
{
public:
    struct by_nodeid
    {
        inline static std::string extract(nodeid_address_unit const& ob)
        {
            return ob.header.node_address;
        }
    };

    struct by_nodeid_and_address
    {
        inline static pair<string, string> extract(nodeid_address_unit const& ob)
        {
            return std::make_pair(ob.header.node_address, ob.header.address.to_string());
        }
    };

    struct by_checked_time_point
    {
        inline static time_point extract(nodeid_address_unit const& ob)
        {
            return ob.header.checked_time_point;
        }
    };

    using type = ::boost::multi_index_container<nodeid_address_unit,
    boost::multi_index::indexed_by<
        boost::multi_index::hashed_non_unique<boost::multi_index::tag<struct by_nodeid>,
            boost::multi_index::global_fun<nodeid_address_unit const&, std::string, &by_nodeid::extract>>,
        boost::multi_index::hashed_unique<boost::multi_index::tag<struct by_nodeid_and_address>,
            boost::multi_index::global_fun<nodeid_address_unit const&, pair<string, string>, &by_nodeid_and_address::extract>>,
        boost::multi_index::ordered_non_unique<boost::multi_index::tag<struct by_checked_time_point>,
            boost::multi_index::global_fun<nodeid_address_unit const&, std::chrono::steady_clock::time_point, &by_checked_time_point::extract>>
    >>;
};

nodeid_address_unit::nodeid_address_unit() = default;
nodeid_address_unit::nodeid_address_unit(nodeid_address_unit&&) = default;
nodeid_address_unit::~nodeid_address_unit() = default;
nodeid_address_unit& nodeid_address_unit::operator = (nodeid_address_unit&&) = default;

namespace detail
{
class nodeid_service_impl
{
public:
    nodeid_container::type nodeids;
};
}

nodeid_service::nodeid_service()
    : m_pimpl(new detail::nodeid_service_impl())
{}
nodeid_service::~nodeid_service() = default;

void nodeid_service::add(std::string const& node_address,
                         beltpp::ip_address const& address,
                         beltpp::ip_address const& ssl_address,
                         std::unique_ptr<session_action_broadcast_address_info>&& ptr_action)
{
    nodeid_address_unit nodeid_item;
    nodeid_item.header.address = address;
    nodeid_item.header.node_address = node_address;
    nodeid_item.verified = nodeid_address_unit::verified_type::never;
    nodeid_item.ssl_address = ssl_address;

    auto it_find = m_pimpl->nodeids.find(nodeid_item.header.node_address);
    if (it_find != m_pimpl->nodeids.end() &&
        it_find->verified == nodeid_address_unit::verified_type::current)
        return;

    auto header_address = nodeid_item.header.address;
    auto header_node_address = nodeid_item.header.node_address;

    auto insert_result = m_pimpl->nodeids.insert(std::move(nodeid_item));
    auto it_nodeid = insert_result.first;

    // there was an old code checking if the item preventing the insert has the same header ip address and node address
    // but this pair is the only unique key for this container
    // just in case, check for logic errors. can remove the checks later, entirelly
    if (it_nodeid->header.address != header_address)
        throw std::logic_error("nodeid_service::add: it_nodeid->header.address != header_address");
    if (it_nodeid->header.node_address != header_node_address)
        throw std::logic_error("nodeid_service::add: it_nodeid->header.node_address != header_node_address");

    /*
    if (insert_result.second ||
        (
            it_nodeid->header.address == nodeid_item.header.address &&
            it_nodeid->header.node_address == nodeid_item.header.node_address
        ))
    */
    {
        bool modified;
        B_UNUSED(modified);
        modified = m_pimpl->nodeids.modify(it_nodeid, [&ptr_action, &ssl_address](nodeid_address_unit& item)
        {
            item.ptr_action = std::move(ptr_action);
            item.ssl_address = ssl_address;
        });
        assert(modified);
    }
}

void nodeid_service::keep_successful(std::string const& node_address,
                                     beltpp::ip_address const& address,
                                     bool verified)
{
    auto it = m_pimpl->nodeids.find(node_address);
    if (it == m_pimpl->nodeids.end())
    {
        assert(false);
        throw std::logic_error("nodeid_service::keep_successful "
            "cannot find the expected nodeid");
    }
    else
    {
        size_t erased_count = 0, kept_count = 0;
        while (it != m_pimpl->nodeids.end() &&
               it->header.node_address == node_address)
        {
            if (it->header.address != address)
            {
                it = m_pimpl->nodeids.erase(it);
                ++erased_count;
            }
            else
            {
                bool modified;
                B_UNUSED(modified);
                modified = m_pimpl->nodeids.modify(it, [verified](nodeid_address_unit& item)
                {
                    item.verified = verified ?
                                        nodeid_address_unit::verified_type::current :
                                        nodeid_address_unit::verified_type::past;
                });
                assert(modified);
                ++it;
                ++kept_count;
            }
        }

        assert(kept_count == 1);
        if (kept_count != 1)
            throw std::logic_error("nodeid_service keep_successful()");

        if (false == verified)
        {
            assert(erased_count == 0);
            if (erased_count != 0)
                throw std::logic_error("nodeid_service keep_successful(false)");
        }

        it = m_pimpl->nodeids.find(node_address);
        if (it == m_pimpl->nodeids.end())
        {
            assert(false);
            throw std::logic_error("this is not possible");
        }
        else
        {
            //  can I do this modify in the while loop above?
            //  maybe it's possible, because this affects different index
            bool modified;
            B_UNUSED(modified);
            modified = m_pimpl->nodeids.modify(it, [](nodeid_address_unit& item)
            {
                item.header.checked_time_point = steady_clock::now();
            });
            assert(modified);
        }
    }
}

void nodeid_service::erase_failed(std::string const& node_address,
                                  beltpp::ip_address const& address)
{
    auto it = m_pimpl->nodeids.find(node_address);
    if (it == m_pimpl->nodeids.end())
    {
        assert(false);
        throw std::logic_error("nodeid_service::erase_failed "
            "cannot find the expected entry");
    }
    else
    {
        size_t erased_count = 0, kept_count = 0;
        while (it != m_pimpl->nodeids.end() &&
               it->header.node_address == node_address)
        {
            if (it->header.address == address)
            {
                it = m_pimpl->nodeids.erase(it);
                ++erased_count;
            }
            else
            {
                ++it;
                ++kept_count;
            }
        }

        B_UNUSED(kept_count);
        B_UNUSED(erased_count);
    }
}

void nodeid_service::take_actions(std::function<void (std::string const&/* node_address*/,
                                                      beltpp::ip_address const&/* address*/,
                                                      std::unique_ptr<session_action_broadcast_address_info>&&/* ptr_action*/)> const& callback)
{
    std::unordered_set<string> prev_node_addresses;

    for (auto it = m_pimpl->nodeids.begin(); it != m_pimpl->nodeids.end(); ++it)
    {
        auto const& nodeid_item = *it;
        if (nodeid_item.ptr_action &&
            0 == prev_node_addresses.count(nodeid_item.header.node_address))
        {
            prev_node_addresses.insert(nodeid_item.header.node_address);

            std::unique_ptr<session_action_broadcast_address_info> ptr_action;

            bool modified;
            B_UNUSED(modified);
            modified = m_pimpl->nodeids.modify(it, [&ptr_action](nodeid_address_unit& item)
            {
                ptr_action = std::move(item.ptr_action);
            });
            assert(modified);

            callback(nodeid_item.header.node_address,
                     nodeid_item.header.address,
                     std::move(ptr_action));
        }
    }
}

BlockchainMessage::PublicAddressesInfo nodeid_service::get_addresses() const
{
    BlockchainMessage::PublicAddressesInfo result;
    BlockchainMessage::PublicAddressesInfo result_verified;

    auto& index_by_checked_time_point = m_pimpl->nodeids.template get<nodeid_container::by_checked_time_point>();

    for (auto it = index_by_checked_time_point.rbegin();
         it != index_by_checked_time_point.rend();
         ++it)
    {
        BlockchainMessage::PublicAddressInfo address_info;
        beltpp::assign(address_info.ip_address, it->header.address);
        address_info.node_address = it->header.node_address;

        beltpp::assign(address_info.ssl_ip_address, it->ssl_address);

        if (it->verified == nodeid_address_unit::verified_type::current)
        {
            address_info.seconds_since_checked = 0;

            result_verified.addresses_info.push_back(std::move(address_info));
        }
        else
        {
            chrono::seconds seconds_since;
            if (it->verified == nodeid_address_unit::verified_type::never)
                seconds_since = chrono::duration_cast<chrono::seconds>(system_clock::now() - system_clock::time_point());
            else
                seconds_since = chrono::duration_cast<chrono::seconds>(steady_clock::now() - it->header.checked_time_point);

            address_info.seconds_since_checked = uint64_t(seconds_since.count());

            result.addresses_info.push_back(std::move(address_info));
        }
    }

    result_verified.addresses_info.insert(result_verified.addresses_info.end(),
                                          result.addresses_info.begin(),
                                          result.addresses_info.end());

    result = std::move(result_verified);

    return result;
}

}// end of namespace publiqpp
