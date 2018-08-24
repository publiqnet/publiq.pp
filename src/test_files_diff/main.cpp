#include <belt.pp/global.hpp>

#include <iostream>
#include <fstream>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

using namespace std;

void showDifferences(const string& str1, const string& str2)
{
    B_UNUSED(str1);
    B_UNUSED(str2);
//    auto itrendstr1 = str1.end();

//    auto itrendstr2 = str2.end();
//    auto itrbeginstr2 = str2.begin();

//    for(auto itrbeginstr1 = str1.begin(); itr!=str1.end(), )

}

string splitLast(const string& path)
{
    auto itr = path.end();
    string result;
    while(itr != path.begin())
    {
        if(*itr != '/')
            result.insert(result.begin(), *itr);
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


        //  definitely there is some bug here
        //  don't write weird code :)

        for (string s, h; getline(first, s), getline(second, h); )
        {
            if (s.compare(h))
            {
                cout<<splitLast(path2)<<endl;
                cout<<"file "<<path1<<" mismatch with file "<<path2<<endl;
                cout << h << "\n " << s << endl;
            }
        }



    }

    vector<string> firstDirs;
    vector<string> firstFiles;

    vector<string> secondDirs;
    vector<string> secondFiles;

    if (fs::is_directory(bfirst) && fs::is_directory(bsecond))
    {
            fs::directory_iterator end_itr;

            fs::directory_iterator itrf(bfirst);
            for ( ;  itrf != end_itr; ++itrf)
            {
                if(fs::is_directory(itrf->path())) {
                    firstDirs.push_back(splitLast(itrf->path().string()));
                }

                if(!fs::is_directory(itrf->path())) {
                    firstFiles.push_back(splitLast(itrf->path().string()));
                }
            }

            fs::directory_iterator itrs(bsecond);
            for ( ;  itrs != end_itr; ++itrs)
            {

                if(fs::is_directory(itrs->path())) {
                    secondDirs.push_back(splitLast(itrs->path().string()));
                }

                if(!fs::is_directory(itrs->path())) {
                    secondFiles.push_back(splitLast(itrs->path().string()));
                }
            }

            sort(firstDirs.begin(), firstDirs.end());
            sort(secondDirs.begin(), secondDirs.end());
            vector<string> dirsDiff;
            set_symmetric_difference(firstDirs.begin(), firstDirs.end(), secondDirs.begin(), secondDirs.end(), back_inserter(dirsDiff));

            if(dirsDiff.size()>0)
            {
                cout << "\n\t\t\tdirs diff \n";
                for(size_t i = 0; i < dirsDiff.size(); ++i)
                {
                    cout << dirsDiff[i] << ",  ";
                }
                cout<<endl;
            }

            sort(firstFiles.begin(), firstFiles.end());
            sort(secondFiles.begin(), secondFiles.end());
            vector<string> filesDiff;
            set_symmetric_difference(firstFiles.begin(), firstFiles.end(), secondFiles.begin(), secondFiles.end(), back_inserter(filesDiff));

            if(filesDiff.size()>0)
            {
                cout << "\n \t\t\tfiles diff \n";
                for(size_t i = 0; i < filesDiff.size(); ++i)
                {
                    cout <<filesDiff[i] << ",  ";
                }
                cout<<endl;

            }

            fs::directory_iterator itrf1(bfirst);
            fs::directory_iterator itrs1(bsecond);
            for ( ;  itrf1 != end_itr && itrs1 != end_itr; ++itrf1, ++itrs1)
            {

                if ( splitLast(itrf1->path().string()) == splitLast(itrs1->path().string()) )
                {

                    compare(itrf1->path().string(), itrs1->path().string());
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

