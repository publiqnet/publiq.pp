set PATH=./install;%PATH%
git clone https://github.com/publiqnet/publiq.pp
mkdir publiq.pp.build
cd publiq.pp
git submodule update --init --recursive
cd ../publiq.pp.build
cmake -G "Visual Studio 15 2017 Win64" -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON -DBUILD_SHARED_LIBS=OFF -DCMAKE_INSTALL_PREFIX=../install ../publiq.pp
REM use the following for VS 2019
REM cmake -A X64 -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON -DBUILD_SHARED_LIBS=OFF -DCMAKE_INSTALL_PREFIX=../install ../publiq.pp
cmake --build . --target install --config Release
cd ..
REM rmdir /Q /S publiq.pp
REM rmdir /Q /S publiq.pp.release