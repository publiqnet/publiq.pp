#pragma once

#include "global.hpp"

#include "message.hpp"

#include <boost/filesystem/path.hpp>

#include <vector>
#include <memory>

namespace publiqpp
{

namespace detail
{
class blockchain_internals;
}

class blockchain
{
public:
    blockchain(boost::filesystem::path const& fs_blockchain);
    ~blockchain();

    size_t length() const;

    void get_last_block(BlockchainMessage::Block& block) const;
    void remove_last_block();
private:
    std::unique_ptr<detail::blockchain_internals> m_pimpl;
};

}
