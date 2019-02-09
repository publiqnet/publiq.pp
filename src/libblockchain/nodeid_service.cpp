#include "nodeid_service.hpp"
#include "common.hpp"
#include "sessions.hpp"

using std::pair;
using std::vector;
using beltpp::ip_address;

namespace publiqpp
{

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

}// end of namespace publiqpp
