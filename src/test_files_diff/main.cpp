#include <iostream>
#include <boost/filesystem.hpp>
using namespace std;
namespace fs = boost::filesystem;

int main(int argc, char* argv[]) {


    if(argc>2) {
        fs::path firstDir(argv[1]);
        fs::path secondDir(argv[2]);

        if( !fs::exists(firstDir) ) {
            cout<<firstDir<<" incorrect path!"<<endl;
            return 0;
        }
        if( !fs::exists(secondDir) ) {
            cout<<secondDir<<" incorrect path!"<<endl;
            return 0;
        }
        if( !fs::is_directory(firstDir) && fs::is_directory(secondDir) ) {
            cout<<"compare file with directory!"<<endl;
            return 0;
        }

        if( fs::is_directory(firstDir) && !fs::is_directory(secondDir) ) {
            cout<<"compare directory with file!"<<endl;
            return 0;
        }

        if( !fs::is_directory(firstDir) && !fs::is_directory(secondDir) ) {
            //compare files
            //show differences
        }

        if( fs::is_directory(firstDir) && fs::is_directory(secondDir)){


                if( fs::is_empty(firstDir) ) {
                    cout<<firstDir<<" is empty!"<<endl;
                    return 0;
                }
                if( fs::is_empty(secondDir) ) {
                    cout<<secondDir<<" is empty!"<<endl;
                    return 0;
                }

            //compare directory recursively
            //show differences
        }
    }
    return 0;
}

