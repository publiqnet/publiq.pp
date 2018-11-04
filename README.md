# publiq.pp

## highlights
### PUBLIQ blockchain node
+ portable executable
+ separate lib, implementing p2p protocol, rpc server, distributed consensus algorithm
+ minimal, tiny http protocol implementation. rpc server provides the same JSON API both over TCP and HTTP (POST)
+ *action log*, interface that allows relational database maintainer to build the blockchain state in any desired DBMS

## details on supported/unsupported features/technologies
+ *dependencies?* [boost](https://www.boost.org "boost"), [mesh.pp](https://github.com/PubliqNetwork/mesh.pp "mesh.pp"), [belt.pp](https://github.com/PubliqNetwork/belt.pp "belt.pp") and [a simple cmake utility](https://github.com/PUBLIQNetwork/cmake_utility "the simple title for the simple cmake utility")
+ *portable?* yes! it's a goal. clang, gcc, msvc working.
+ *build system?* cmake. refer to cmake project generator options for msvc, xcode, etc... project generation.
+ *static linking vs dynamic linking?* both supported, refer to BUILD_SHARED_LIBS cmake option
+ *32 bit build?* never tried to fix build errors/warnings
+ c++11
+ *unicode support?* boost::filesystem::path ensures that unicode paths are well supported and the rest of the code assumes that std::string is utf8 encoded.
+ *full blown blockchain?*, yes! we hope to solve the distributed consensus algorithm fine details and all the blockchain communication to serve the needs of PUBLIQ protocol soon.

## how to build publiq.pp?
```
cd to_somewhere_else
git clone https://github.com/PubliqNetwork/publiq.pp
cd publiq.pp
git submodule update --init --recursive
cd ..
mkdir publiq.pp.build
cd publiq.pp.build
cmake -DCMAKE_INSTALL_PREFIX=./install ../publiq.pp
cmake --build . --target install
```

### git submodules?
yes, we keep up with belt.pp and mesh.pp. those are essential parts of the project, so, if you're a developer contributing to mesh.pp and belt.pp too, then
```
cd publiq.pp
cd src/belt.pp
git checkout master
git pull
cd ../..
cd src/mesh.pp
cd src/belt.pp
git checkout master
git pull
cd ../..
git checkout master
git pull
```
as you see, belt.pp appears twice as a submodule, the other one that relies under mesh.pp, does not participate in the build process, we just keep up with it, to have proper repository history

### how to use publiqd?
there is a command line arguments help, use it wisely!
also pay attention to all the console logging.
everything's is in an active development stage.

### more details?
refer to mesh.pp and belt.pp readme files
