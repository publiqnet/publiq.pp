#pragma once

#include "global.hpp"
#include "message.hpp"

#include <mesh.pp/p2psocket.hpp>

#include <unordered_map>
#include <vector>
#include <utility>
#include <memory>

namespace publiqpp
{
class session_action_broadcast_address_info;
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
    friend class session_action_signatures;
public:
    void add(beltpp::ip_address const& address,
             std::unique_ptr<session_action_broadcast_address_info>&& ptr_action);
    std::vector<beltpp::ip_address> get() const;
    std::unique_ptr<session_action_broadcast_address_info> take_action(beltpp::ip_address const& address);
    bool is_verified(beltpp::ip_address const& address) const;

private:
    std::vector<nodeid_address_unit> addresses;
};

class nodeid_service
{
public:
    std::unordered_map<meshpp::p2psocket::peer_id, nodeid_address_info> nodeids;
};
}// end of namespace publiqpp
