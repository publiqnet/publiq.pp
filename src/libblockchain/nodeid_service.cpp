#include "nodeid_service.hpp"
#include "common.hpp"
#include "sessions.hpp"

#include <unordered_map>
#include <vector>
#include <utility>

using std::pair;
using std::vector;
using beltpp::ip_address;

namespace publiqpp
{

class nodeid_address_unit
{
public:
    nodeid_address_unit();
    nodeid_address_unit(nodeid_address_unit&&);
    ~nodeid_address_unit();

    nodeid_address_unit& operator = (nodeid_address_unit&&);

    beltpp::ip_address address;
    std::unique_ptr<session_action_broadcast_address_info> ptr_action;
    bool verified;
};

class nodeid_address_info
{
    friend class nodeid_service;
public:
    void add(beltpp::ip_address const& address,
             std::unique_ptr<session_action_broadcast_address_info>&& ptr_action);
    std::vector<beltpp::ip_address> get() const;
    std::unique_ptr<session_action_broadcast_address_info> take_action(beltpp::ip_address const& address);
    bool is_verified(beltpp::ip_address const& address) const;

private:
    std::vector<nodeid_address_unit> addresses;
};

nodeid_address_unit::nodeid_address_unit() = default;
nodeid_address_unit::nodeid_address_unit(nodeid_address_unit&&) = default;
nodeid_address_unit::~nodeid_address_unit() = default;
nodeid_address_unit& nodeid_address_unit::operator = (nodeid_address_unit&&) = default;

void nodeid_address_info::add(beltpp::ip_address const& address,
                              std::unique_ptr<session_action_broadcast_address_info>&& ptr_action)
{
    if (2 == addresses.size() ||
        (
            1 == addresses.size() &&
            (
                true == addresses.front().verified ||
                addresses.front().address == address
            )
        ))
        return;

    nodeid_address_unit unit;
    unit.address = address;
    unit.ptr_action = std::move(ptr_action);
    unit.verified = false;

    addresses.emplace_back(std::move(unit));
}

vector<ip_address> nodeid_address_info::get() const
{
    vector<ip_address> result;

    if (1 <= addresses.size())
        result.push_back(addresses.front().address);
    if (2 == addresses.size() &&
        false == addresses.front().verified)
        result.push_back(addresses.back().address);

    return result;
}
std::unique_ptr<session_action_broadcast_address_info>
nodeid_address_info::take_action(beltpp::ip_address const& address)
{
    std::unique_ptr<session_action_broadcast_address_info> result;
    for (auto& item : addresses)
    {
        if (item.address == address)
        {
            result = std::move(item.ptr_action);
            break;
        }
    }

    return result;
}
bool nodeid_address_info::is_verified(beltpp::ip_address const& address) const
{
    for (auto& item : addresses)
    {
        if (item.address == address)
            return item.verified;
    }
    return false;
}

namespace detail
{
class nodeid_service_impl
{
public:
    std::unordered_map<meshpp::p2psocket::peer_id, nodeid_address_info> nodeids;
};
}

nodeid_service::nodeid_service()
    : m_pimpl(new detail::nodeid_service_impl())
{}
nodeid_service::~nodeid_service() = default;

void nodeid_service::add(std::string const& node_address,
                         beltpp::ip_address const& address,
                         std::unique_ptr<session_action_broadcast_address_info>&& ptr_action)
{
    nodeid_address_info& nodeid_info = m_pimpl->nodeids[node_address];

    if (false == nodeid_info.addresses.empty() &&
        nodeid_info.addresses.front().verified)
        return;

    nodeid_info.add(address, std::move(ptr_action));
}

void nodeid_service::keep_successful(std::string const& node_address,
                                     beltpp::ip_address const& address,
                                     bool verified)
{
    auto it = m_pimpl->nodeids.find(node_address);
    if (it == m_pimpl->nodeids.end())
    {
        assert(false);
        throw std::logic_error("session_action_signatures::erase "
            "cannot find the expected entry");
    }
    else
    {
        auto& array = it->second.addresses;

        if (false == verified)
        {
            //  there must be only a single element and be in verified state
            assert(array.size() == 1);
            if (array.size() != 1)
                throw std::logic_error("nodeid_service keep_successful(false)");
            array.front().verified = false;
        }
        else
        {
            auto it_end = std::remove_if(array.begin(), array.end(),
                [&address](nodeid_address_unit const& unit)
            {
                return unit.address != address;
            });
            array.erase(it_end, array.end());

            assert(array.size() == 1);
            if (array.size() != 1)
                throw std::logic_error("nodeid_service keep_successful(true)");
            array.front().verified = true;
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
        throw std::logic_error("session_action_signatures::erase "
            "cannot find the expected entry");
    }
    else
    {
        auto& array = it->second.addresses;
        auto it_end = std::remove_if(array.begin(), array.end(),
            [&address](nodeid_address_unit const& unit)
        {
            return unit.address == address;
        });
        array.erase(it_end, array.end());
    }
}


void nodeid_service::take_actions(std::function<void (std::string const& node_address,
                                                      beltpp::ip_address const& address,
                                                      std::unique_ptr<session_action_broadcast_address_info>&& ptr_action)> const& callback)
{
    for (auto& nodeid_item : m_pimpl->nodeids)
    {
        publiqpp::nodeid_address_info& address_info = nodeid_item.second;
        vector<ip_address> addresses = address_info.get();
        if (false == addresses.empty())
        {
            auto& address = addresses.front();
            auto ptr_action = nodeid_item.second.take_action(address);

            if (ptr_action)
                callback(nodeid_item.first, address, std::move(ptr_action));
        }
    }
}

}// end of namespace publiqpp
