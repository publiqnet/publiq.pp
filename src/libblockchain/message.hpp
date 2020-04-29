#pragma once
#include "global.hpp"

//  for now workaround optional support like this
//  later try to make it more or less as it should
#include <boost/optional.hpp>

namespace BlockchainMessage
{
using boost::optional;
}
#include "message.gen.hpp"
