# publiq.pp

## highlights
### PUBLIQ blockchain node
+ portable executable
+ separate lib, implementing p2p protocol, rpc server, distributed consensus algorithm
+ minimal, tiny http protocol implementation. rpc server provides the same JSON API both over TCP and HTTP (POST)
+ *action log*, interface that allows relational database maintainer to build the blockchain state in any desired DBMS

## details on supported/unsupported features/technologies
+ *dependencies?* [boost](https://www.boost.org "boost"), [mesh.pp](https://github.com/publiqnet/mesh.pp "mesh.pp"), [belt.pp](https://github.com/publiqnet/belt.pp "belt.pp") and [a simple cmake utility](https://github.com/publiqnet/cmake_utility "the simple title for the simple cmake utility")
+ *portable?* yes! it's a goal. clang, gcc, msvc working.
+ *build system?* cmake. refer to cmake project generator options for msvc, xcode, etc... project generation.
+ *static linking vs dynamic linking?* both supported, refer to BUILD_SHARED_LIBS cmake option
+ *32 bit build?* never tried to fix build errors/warnings
+ c++11
+ *unicode support?* boost::filesystem::path ensures that unicode paths are well supported and the rest of the code assumes that std::string is utf8 encoded.
+ *full blown blockchain?*, yes! we hope to solve the rough edges around distributed consensus algorithm and all the blockchain communication to serve the needs of PUBLIQ protocol soon.

## how to build publiq.pp?
```console
user@pc:~$ mkdir projects
user@pc:~$ cd projects
user@pc:~/projects$ git clone https://github.com/publiqnet/publiq.pp
user@pc:~/projects$ cd publiq.pp
user@pc:~/projects/publiq.pp$ git submodule update --init --recursive
user@pc:~/projects/publiq.pp$ cd ..
user@pc:~/projects$ mkdir publiq.pp.build
user@pc:~/projects$ cd publiq.pp.build
user@pc:~/projects/publiq.pp.build$ cmake -DCMAKE_INSTALL_PREFIX=./install ../publiq.pp
user@pc:~/projects/publiq.pp.build$ cmake --build . --target install
```

### git submodules?
yes, we keep up with belt.pp and mesh.pp. those are essential parts of the project, so, if you're a developer contributing to mesh.pp and belt.pp too, then
```console
user@pc:~$ cd projects/publiq.pp
user@pc:~/projects$ cd src/belt.pp
user@pc:~/projects/src/belt.pp$ git checkout master
user@pc:~/projects/src/belt.pp$ git pull
user@pc:~/projects/src/belt.pp$ cd ../..
user@pc:~/projects$ cd src/mesh.pp
user@pc:~/projects/src/mesh.pp$ cd src/belt.pp
user@pc:~/projects/src/mesh.pp/src/belt.pp$ git checkout master
user@pc:~/projects/src/mesh.pp/src/belt.pp$ git pull
user@pc:~/projects/src/mesh.pp/src/belt.pp$ cd ../..
user@pc:~/projects/src/mesh.pp$ git checkout master
user@pc:~/projects/src/mesh.pp$ git pull
```
as you see, belt.pp appears twice as a submodule, the other one that relies under mesh.pp, does not participate in the build process, we just keep up with it, to have proper repository history

### how to use publiqd?
there is a command line arguments help, use it wisely!
also pay attention to all the console logging.
everything's is in an active development stage.

### more details?
refer to mesh.pp and belt.pp readme files
