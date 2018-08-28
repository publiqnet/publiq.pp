#include <belt.pp/global.hpp>

#include <iostream>
#include <fstream>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

using namespace std;

int diffPossition(const string& str1, const string& str2)
{
    auto it1 = str1.begin();
    auto it2 = str2.begin();
    int i=0;
    while (it1 != str1.end()  && it2 != str2.end())
    {
        if(*it1 != *it2)
        {
            return i;
        }
        it1++;
        it2++;
        i++;
    }
    return i;
}

bool isSame(const string& path1, const string&  path2)
{
    ifstream first(path1);
    ifstream second(path2);

    fs::path bfirst(path1);
    fs::path bsecond(path2);


    if (!fs::exists(bfirst))
    {
       cout<<bfirst<<" incorrect path!"<<endl;
       return false;
    }
    if (!fs::exists(bsecond))
    {
       cout<<bsecond<<" incorrect path!"<<endl;
       return false;
    }

    if (!fs::is_directory(bfirst) && fs::is_directory(bsecond))
    {
        cout<<"compare file with directory!"<<endl;
        return false;
    }

    if (fs::is_directory(bfirst) && !fs::is_directory(bsecond))
    {
        cout<<"compare directory with file!"<<endl;
        return false;
    }

    bool static flag = true;
    static vector<string> mismathedFiles;
    if (!fs::is_directory(bfirst) && !fs::is_directory(bsecond))
    {      
        size_t i = 0;
        string l1, l2;
        while ( getline(first, l1)  &&  getline(second, l2) )
        {
            i++;
            if (l1.compare(l2))
            {
                string lineconut = to_string(i);
                string possition = to_string( diffPossition(l1, l2) );
                string s =   "file " + path1 + " mismatch with file " + path2 + "\nDifference in line " + lineconut + ": " + possition  + "-th possition\n\n";
                mismathedFiles.push_back(s);
                flag = false;
                return flag;
            }

         }
    }

    vector<string> firstDirs;
    vector<string> firstFiles;

    vector<string> secondDirs;
    vector<string> secondFiles;

    if ( fs::is_directory(bfirst) && fs::is_directory(bsecond) )
    {
            fs::directory_iterator itrf(bfirst);
            auto end_itr = fs::end(itrf);

            for ( ;  itrf != end_itr; ++itrf)
            {
                if( fs::is_directory(itrf->path()) )
                {
                    fs::path fileName(itrf->path().string());
                    firstDirs.push_back(fileName.leaf().string());
                }

                if( !fs::is_directory(itrf->path()) )
                {
                    fs::path fileName(itrf->path().string());
                    firstFiles.push_back(fileName.leaf().string());
                }
            }

            fs::directory_iterator itrs(bsecond);
            end_itr = fs::end(itrs);

            for ( ;  itrs != end_itr; ++itrs)
            {
               if(fs::is_directory(itrs->path()))
                {
                   fs::path fileName(itrs->path().string());
                   secondDirs.push_back(fileName.leaf().string());
                }

                if(!fs::is_directory(itrs->path()))
                {
                    fs::path fileName(itrs->path().string());
                    secondFiles.push_back(fileName.leaf().string());
                }
            }

            sort(firstDirs.begin(), firstDirs.end());
            sort(secondDirs.begin(), secondDirs.end());
            vector<string> dirsDiff;
            set_symmetric_difference(firstDirs.begin(), firstDirs.end(), secondDirs.begin(), secondDirs.end(), back_inserter(dirsDiff));

            if(dirsDiff.size()>0)
            {

                flag = false;
                cout << "\n~~~~~~~~~~~~~~~~~~~~~The following directores does not exist in one of the given path~~~~~~~~~\n\n";
                for(size_t i = 0; i<dirsDiff.size(); ++i)
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
                flag = false;
                if(filesDiff.size()>10)
                {
                    flag = false;
                    cout << "\n\n~~~~~~~~~~~~~~~~~~~~~   The files does not exist in one of the given path     ~~~~~~~~~~~~~";
                    cout<<"\nThe "<<filesDiff.size()<<" files dose not exist, same of them:\n\n ";
                    for(size_t i=0; i<10; ++i)
                    {
                        if(filesDiff[1] != " .DS_Store")
                        {
                             cout << filesDiff[i] << ",  ";
                        }
                    }
                }
                else
                {
                    cout << "\n\n~~~~~~~~~~~~~~~~~~~~~    The following files does not exist in one of the given path     ~~~~~~~~~~~~~\n\n";
                    for(size_t i = 0; i<filesDiff.size(); ++i)
                    {
                         cout << filesDiff[i] << ",  ";
                    }
                }
            }

            fs::directory_iterator itrf1(bfirst);
            fs::directory_iterator itrs1(bsecond);
            vector<string> firstStructure;
            vector<string> secondStructure;
            for ( ;  itrf1 != end_itr && itrs1 != end_itr; ++itrf1, ++itrs1)
            {
                firstStructure.push_back(itrf1->path().string());
                secondStructure.push_back(itrs1->path().string());
            }

            sort(firstStructure.begin(), firstStructure.end());
            sort(secondStructure.begin(), secondStructure.end());

            auto it1 = firstStructure.begin();
            auto it2 = secondStructure.begin();
            for ( ; it1 != firstStructure.end() && it2 != secondStructure.end(); ++it1, ++it2)
            {
                fs::path name1(*it1);
                fs::path name2(*it2);

                if ( name1.leaf().string() == name2.leaf().string() )
                {
                    isSame(*it1, *it2);
                }
            }
    }

    static bool once = false;
    if(mismathedFiles.size()>0)
    {
        if(!once){

           once = true;
           cout<<"\n\n~~~~~~~~~~~~~~~~~~~~~~~~    Files mismath    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ \n";
           if(mismathedFiles.size() > 10)
           {
               cout<<"\nThe "<< mismathedFiles.size() <<" files mismathed, same of them:\n\n ";
               for(size_t i = 0; i < 10; ++i)
               {
                        cout << mismathedFiles[i];
               }
               return flag;
           }
           else
           {
               cout<<"\nThe "<< mismathedFiles.size() <<" files mismathed, same of them:\n\n ";
               for(size_t i = 0; i < mismathedFiles.size(); ++i)
               {
                    cout << mismathedFiles[i];
               }
               return flag;
           }
        }

    }

    return flag;
}

int main(int argc, char* argv[]) 
{
    if (argc>2)
    {
        string path1 = argv[1];
        string path2 = argv[2];

        if ( isSame(path1, path2) )
        {
            cout<< "identical structures" <<endl;
        }
    }
    return 0;
}

