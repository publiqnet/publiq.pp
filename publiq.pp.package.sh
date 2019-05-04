PATH=./install:$PATH
git clone https://github.com/publiqnet/publiq.pp
mkdir publiq.pp.release
cd publiq.pp
git submodule update --init --recursive
cd ../publiq.pp.release
cmake -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON -DBUILD_SHARED_LIBS=OFF -DCMAKE_INSTALL_PREFIX=../install -DCMAKE_BUILD_TYPE=Release ../publiq.pp
#make install -j 8
cmake --build . --target install -- -j 8
cd ..
rm -rf publiq.pp
rm -rf publiq.pp.release

