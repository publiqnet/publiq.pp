#include "blockchain.hpp"

#include "message.hpp"

#include <boost/filesystem.hpp>

namespace filesystem = boost::filesystem;

class blockchain_ex : public publiqpp::blockchain
{
public:
    blockchain_ex()
    {

    }

    virtual ~blockchain_ex() {}
};

namespace publiqpp
{
blockchain_ptr getblockchain()
{
    return beltpp::new_dc_unique_ptr<blockchain, blockchain_ex>();
}
}
