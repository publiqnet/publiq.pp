wget https://boostorg.jfrog.io/artifactory/main/release/1.82.0/source/boost_1_82_0.tar.gz
tar xvzf boost_1_82_0.tar.gz
rm boost_1_82_0.tar.gz
cd boost_1_82_0
./bootstrap.sh cxxflags="-stdlib=libc++ -std=c++11" linkflags="-stdlib=libc++ -std=c++11"
# toolset=clang cxxflags="-stdlib=libc++ -std=c++11" linkflags="-stdlib=libc++ -std=c++11"
# boost.locale.winapi=off boost.locale.std=off boost.locale.posix=off boost.locale.iconv=on boost.locale.icu=off
# variant=release
# --ignore-site-config
# link=static
./b2 install -j 8 --ignore-site-config link=static variant=release boost.locale.winapi=off boost.locale.std=off boost.locale.posix=off boost.locale.iconv=on boost.locale.icu=off --with-program_options --with-system --with-filesystem --with-locale --prefix=../install --build-dir=../boost-build
cd ..
rm -rf boost_1_82_0
rm -rf boost-build

