#include <iostream>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

using namespace std;

string splitLast(const string& path)
{
    auto itr = path.end();
    string result;
    while(itr != path.begin())
    {
            if(*itr != '/')
                result.push_back(*itr);
            else if (*itr == '/'){
                return result;
            }
            --itr;
     }
        return result;
}

void compare(const string& path1, const string&  path2)
{
    ifstream first(path1);
    ifstream second(path2);

    fs::path bfirst(path1);
    fs::path bsecond(path2);


    if (!fs::exists(bfirst))
    {
        cout<<bfirst<<" incorrect path!"<<endl;
        return;
    }

    if (!fs::exists(bsecond))
    {
        cout<<bsecond<<" incorrect path!"<<endl;
        return;
    }

    if (!fs::is_directory(bfirst) && fs::is_directory(bsecond))
    {
        cout<<"compare file with directory!"<<endl;
        return;
    }

    if (fs::is_directory(bfirst) && !fs::is_directory(bsecond))
    {
        cout<<"compare directory with file!"<<endl;
        return;
    }

    if (!fs::is_directory(bfirst) && !fs::is_directory(bsecond))
    {

        for (string s; getline(first, s); )
        {
            for (string h; getline(second,h); )
            {
                if (s.compare(h))
                {
                    cout<<"file "<<path1<<" mismatch with file "<<path2<<endl;
                    cout << h << "\n " << s << endl;
                 }
             }
        }
    }

    if (fs::is_directory(bfirst) && fs::is_directory(bsecond))
    {
            boost::filesystem::directory_iterator end_itr;
            boost::filesystem::directory_iterator itrf(bfirst);
            boost::filesystem::directory_iterator itrs(bsecond);

            for ( ;  itrf != end_itr && itrs != end_itr; ++itrf, ++itrs)
            {
                    if ( splitLast(itrf->path().string()) == splitLast(itrs->path().string()) )
                    {
                        compare(itrf->path().string(), itrs->path().string());
                    }
            }
    }
}

int main(int argc, char* argv[]) 
{

    if (argc>2)
    {
        string path1 = argv[1];
        string path2 = argv[2];
        compare(path1, path2);
    }

    return 0;
}

