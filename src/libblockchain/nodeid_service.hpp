#pragma once

#include "global.hpp"

#include <mesh.pp/p2psocket.hpp>

#include <memory>
#include <functional>

namespace publiqpp
{
class session_action_broadcast_address_info;

namespace detail
{
    class nodeid_service_impl;
}

class nodeid_service
{
public:
    nodeid_service();
    nodeid_service(nodeid_service const&) = delete;
    ~nodeid_service();

    void add(std::string const& node_address,
             beltpp::ip_address const& address,
             std::unique_ptr<session_action_broadcast_address_info>&& ptr_action);

    void keep_successful(std::string const& node_address,
                         beltpp::ip_address const& address,
                         bool verified);
    void erase_failed(std::string const& node_address,
                      beltpp::ip_address const& address);

    void take_actions(std::function<void (std::string const& node_address,
                                          beltpp::ip_address const& address,
                                          std::unique_ptr<session_action_broadcast_address_info>&& ptr_action)> const& callback);
private:
    std::unique_ptr<detail::nodeid_service_impl> m_pimpl;
};
}// end of namespace publiqpp
