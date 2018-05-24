#include "blockchainstate.hpp"

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

class blockchainstate_ex : public publiqpp::blockchainstate
{
public:
    blockchainstate_ex()
    {
    }

    virtual ~blockchainstate_ex() {}
};

namespace publiqpp
{
blockchainstate_ptr getblockchainstate()
{
    return beltpp::new_dc_unique_ptr<blockchainstate, blockchainstate_ex>();
}
}
