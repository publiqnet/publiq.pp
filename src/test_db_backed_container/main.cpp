#include "../libblockchain/message.hpp"

#include <belt.pp/global.hpp>

#include <mesh.pp/rdbutility.hpp>

#include <iostream>
#include <chrono>
#include <string>
#include <sstream>

using namespace std;
using chrono::steady_clock;

template <class T>
struct modem
{
    string serialize(T const& v) const { stringstream ss; ss << v; return ss.str(); }
    T deserialize(string const& s) const { T v; stringstream ss(s); ss >> v; return v; }
};

#define TEST_WRITE

int main(int argc, char** argv)
{
    try
    {
        if (argc < 4)
        {
            cout << "usage: test_loader_simulation folder index_from index_to" << endl;
            return 0;
        }

        string path = argv[1];
        size_t pos;
        size_t index_from = size_t(beltpp::stoi64(argv[2], pos));
        size_t index_to = size_t(beltpp::stoi64(argv[3], pos));

        cout << "path: " << path << endl;
        cout << "index from: " << index_from << endl;
        cout << "index to: " << index_to << endl;

        auto tp_start = steady_clock::now();

        meshpp::db_loader<string, modem<string>> actions(path, "actions");

        for (size_t index = index_from; index < index_to; ++index)
        {
#ifdef TEST_WRITE
            actions.push_back(to_string(index));
            actions.save();
            actions.commit();
#else
            try
            {
                auto s = actions.as_const().at(index) ;
            }
            catch(std::exception const& e)
            {
                cout << "exception: " << e.what() << endl;
            }
#endif
        }

        auto duration = steady_clock::now() - tp_start;
        cout<<endl;
        auto  ms_duration = chrono::duration_cast<chrono::milliseconds>(duration);
        cout << ms_duration.count() << " ms" << endl;
    }
    catch(std::exception const& e)
    {
        cout << "exception: " << e.what() << endl;
    }

    return 0;
}
