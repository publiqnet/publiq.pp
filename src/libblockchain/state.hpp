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
class state_internals;
}
class state
{
public:
    state(boost::filesystem::path const& fs_state);
    ~state();

    void save();
    void commit();
    void discard();

    BlockchainMessage::Coin get_balance(std::string const& key) const;

    void apply_transfer(BlockchainMessage::Transfer const& transfer, BlockchainMessage::Coin const& fee);
    void increase_balance(std::string const& key, coin const& amount);
    void decrease_balance(std::string const& key, coin const& amount);

    std::vector<std::string> get_nodes_by_type(BlockchainMessage::NodeType const& node_type) const;
    bool get_role(std::string const& nodeid, BlockchainMessage::NodeType& node_type) const;
    void insert_role(BlockchainMessage::Role const& role);
    void remove_role(std::string const& nodeid);

private:
    std::unique_ptr<detail::state_internals> m_pimpl;
};
}
