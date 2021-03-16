git clone https://github.com/weidai11/cryptopp --depth=1
cd cryptopp
git submodule add https://github.com/noloader/cryptopp-cmake cmake
git submodule update --remote
copy cmake\cryptopp-config.cmake .
copy cmake\CMakeLists.txt .
cd ..
mkdir cryptopp.build
cd cryptopp.build
cmake -A X64 -DCMAKE_INSTALL_PREFIX=../install ../cryptopp
cmake --build . --target install --config Release
cd ..
rmdir /Q /S cryptopp
rmdir /Q /S cryptopp.build
ren install\lib\cryptopp-static.lib cryptopp.lib