#include "blockchainstate.hpp"


class blockchainstate_ex : public publiqpp::blockchainstate
{
public:
    blockchainstate_ex() {}

    virtual ~blockchainstate_ex() {}
};

namespace publiqpp
{
blockchainstate_ptr getblockchainstate()
{
    return beltpp::new_dc_unique_ptr<blockchainstate, blockchainstate_ex>();
}
}
