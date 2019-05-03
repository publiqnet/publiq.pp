wget https://dl.bintray.com/boostorg/release/1.70.0/source/boost_1_70_0.tar.gz
tar xvzf boost_1_70_0.tar.gz
rm boost_1_70_0.tar.gz
cd boost_1_70_0
./bootstrap.sh
# toolset=clang cxxflags="-stdlib=libc++ -std=c++11" linkflags="-stdlib=libc++ -std=c++11"
# boost.locale.winapi=off boost.locale.std=off boost.locale.posix=off boost.locale.iconv=on boost.locale.icu=off
# variant=release
./b2 install -j 8 link=static variant=release boost.locale.winapi=off boost.locale.std=off boost.locale.posix=off boost.locale.iconv=on boost.locale.icu=off --with-program_options --with-system --with-filesystem --with-locale --prefix=../install --build-dir=../boost-build
cd ..
rm -rf boost_1_70_0
rm -rf boost-build
BOOST_ROOT=./install
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

