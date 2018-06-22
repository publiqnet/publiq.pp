#include "state.hpp"
#include "blockchain.hpp"
#include "action_log.hpp"
#include "storage.hpp"

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

namespace filesystem = boost::filesystem;
using std::unique_ptr;

namespace publiqpp
{
namespace detail
{
class state_internals
{
public:
    state_internals(filesystem::path const& fs_blockchain,
                    filesystem::path const& fs_action_log,
                    filesystem::path const& fs_storage)
        : m_blockchain(fs_blockchain)
        , m_action_log(fs_action_log)
        , m_storage(fs_storage)
    {}

    publiqpp::blockchain m_blockchain;
    publiqpp::action_log m_action_log;
    publiqpp::storage m_storage;
};
}

state::state(filesystem::path const& fs_blockchain,
             filesystem::path const& fs_action_log,
             filesystem::path const& fs_storage)
    : m_pimpl(new detail::state_internals(fs_blockchain,
                                          fs_action_log,
                                          fs_storage))
{

}

state::~state()
{

}

publiqpp::blockchain& state::blockchain()
{
    return m_pimpl->m_blockchain;
}

publiqpp::action_log& state::action_log()
{
    return m_pimpl->m_action_log;
}

publiqpp::storage& state::storage()
{
    return m_pimpl->m_storage;
}
}
