#include "../libblockchain/message.hpp"

#include <belt.pp/global.hpp>

#include <mesh.pp/fileutility.hpp>

#include <iostream>
#include <chrono>
#include <string>
#include <fstream>

using namespace BlockchainMessage;

using std::cout;
using std::endl;
namespace chrono = std::chrono;
using std::chrono::steady_clock;
using std::string;

int main(int argc, char** argv)
{
    std::ifstream fl("/Users/tigran/blockchain.9771");
    std::istreambuf_iterator<char> begin(fl), end;
    string str(begin, end);
    SignedBlock sb;

    auto const count = 1000;
    steady_clock::time_point start = steady_clock::now();

    for (size_t index = 0; index < count; ++index)
    {
        sb.from_string(str);
    }
    chrono::milliseconds duration =
            chrono::duration_cast<chrono::milliseconds>(steady_clock::now() - start);

    cout << duration.count() << " milliseconds" << endl;

    return 0;
}
