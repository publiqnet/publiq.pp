#include <publiq.pp/message.hpp>

#include <belt.pp/global.hpp>

#include <mesh.pp/rdbutility.hpp>

#include <iostream>
#include <chrono>
#include <string>
#include <sstream>

using namespace std;
using chrono::steady_clock;

template <typename T>
struct modem
{
    string serialize(T const& v) const { std::stringstream ss; ss << v; return ss.str(); }
    T deserialize(string const& s) const { T v; std::stringstream ss(s); ss >> v; return v; }
};

template <>
struct modem<string>
{
    string serialize(string const& v) const { return v; }
    string deserialize(string const& s) const { return s; }
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

        //db_vector
        {
            auto tp_start = steady_clock::now();

            meshpp::db_vector<string, modem> actions(path, "vector_test");

            for (size_t index = index_from; index < index_to; ++index)
            {
#ifdef TEST_WRITE
                actions.push_back(std::to_string(index));
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

        // db_map
        {
            auto tp_start = steady_clock::now();

            meshpp::db_map<size_t, string, modem> actions(path, "map_test");

            for (size_t index = index_from; index < index_to; ++index)
            {
#ifdef TEST_WRITE
                actions.insert(index, std::to_string(index));
                actions.save();
                actions.commit();
#else
                try
                {
                    auto s = actions.as_const().at(index);
                    if(index % 1001 == 0)
                        cout << s;
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

    }
    catch(std::exception const& e)
    {
        cout << "exception: " << e.what() << endl;
    }

    return 0;
}
