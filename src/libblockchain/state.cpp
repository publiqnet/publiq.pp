#include "state.hpp"

#include "message.hpp"

#include <mesh.pp/fileutility.hpp>

#include <string>

namespace filesystem = boost::filesystem;
using std::string;
using std::vector;

namespace publiqpp
{
namespace detail
{

class state_internals
{
public:
    state_internals(filesystem::path const& path)
        : m_path(path)
    {}

    filesystem::path m_path;
};
}

state::state(filesystem::path const& fs_state)
    : m_pimpl(new detail::state_internals(fs_state))
{
}

state::~state()
{
}

std::vector<string> state::accounts() const
{
    return vector<string>();
}

void state::set_balance(std::string const&/* pb_key*/, uint64_t/* amount*/)
{

}
uint64_t state::get_balance(std::string const&/* key*/) const
{
    return 0;
}

}
