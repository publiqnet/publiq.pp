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
class storage_internals;
}

class storage
{
public:
    storage(boost::filesystem::path const& fs_storage);
    ~storage();
private:
    std::unique_ptr<detail::storage_internals> m_pimpl;
};

}
