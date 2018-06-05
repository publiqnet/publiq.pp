#include "state.hpp"
#include "blockchain.hpp"

#include <cryptopp/sha.h>
#include <cryptopp/filters.h>
#include <cryptopp/base64.h>

std::string SHA256HashString(std::string aString){
    std::string digest;
    CryptoPP::SHA256 hash;

    CryptoPP::StringSource foo(aString, true,
    new CryptoPP::HashFilter(hash,
      new CryptoPP::Base64Encoder (
         new CryptoPP::StringSink(digest))));

    return digest;
}

class state_ex : public publiqpp::state
{
public:
    state_ex(boost::filesystem::path const& fs_blockchain)
        : m_ptr_blockchain(publiqpp::getblockchain(fs_blockchain))
    {
    }

    virtual ~state_ex() {}

    publiqpp::blockchain_ptr m_ptr_blockchain;
};

namespace publiqpp
{
state_ptr getstate(boost::filesystem::path const& fs_blockchain)
{
    return beltpp::new_dc_unique_ptr<state, state_ex>(fs_blockchain);
}
}
