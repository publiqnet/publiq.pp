# publiq.pp

## highlights
### PUBLIQ blockchain node
+ portable executable
+ separate library, implementing p2p protocol, rpc server, distributed consensus algorithm
+ minimal, tiny http protocol implementation. rpc server provides the same JSON API both over TCP and HTTP (POST)
+ *action log*, interface that allows relational database maintainer to build the blockchain state in any desired DBMS
### commander
+ separate RPC JSON and HTTP APIs
+ blockchain explorer
+ wallet functionality

## details on supported/unsupported features/technologies
+ *dependencies?* [boost](https://www.boost.org "boost"), [mesh.pp](https://github.com/publiqnet/mesh.pp "mesh.pp"), [belt.pp](https://github.com/publiqnet/belt.pp "belt.pp") and [a simple cmake utility](https://github.com/publiqnet/cmake_utility "the simple title for the simple cmake utility"). [crypto++](https://www.cryptopp.com/ "crypto++") is another dependency of mesh.pp.
+ *portable?* yes! it's a goal. clang, gcc, msvc working.
+ *build system?* cmake. refer to cmake project generator options for msvc, xcode, etc... project generation.
+ *static linking vs dynamic linking?* both supported, refer to BUILD_SHARED_LIBS cmake option
+ *32 bit build?* never tried to fix build errors/warnings on windows. tested successfully on linux.
+ c++11
+ *unicode support?* boost::filesystem::path ensures that unicode paths are well supported and the rest of the code assumes that std::string is utf8 encoded.
+ *full blown blockchain?*, yes! it is all inside this code repository. all from scratch.

## how to build publiq.pp?
in case the following does not really get you started, please refer to [wiki](https://github.com/publiqnet/publiq.pp/wiki/1.1-Home "wiki")
```console
user@pc:~$ mkdir projects
user@pc:~$ cd projects
user@pc:~/projects$ git clone https://github.com/publiqnet/publiq.pp
user@pc:~/projects$ cd publiq.pp
user@pc:~/projects/publiq.pp$ git submodule update --init --recursive
user@pc:~/projects/publiq.pp$ cd ..
user@pc:~/projects$ mkdir publiq.pp.build
user@pc:~/projects$ cd publiq.pp.build
user@pc:~/projects/publiq.pp.build$ cmake -DCMAKE_INSTALL_PREFIX=./install -DCMAKE_BUILD_TYPE=Release ../publiq.pp
user@pc:~/projects/publiq.pp.build$ cmake --build . --target install
```

### git submodules?
yes, we keep up with belt.pp and mesh.pp. those are essential parts of the project, so, if you're a developer contributing to mesh.pp and belt.pp too, then
```console
user@pc:~$ cd projects/publiq.pp
user@pc:~/projects/publiq.pp$ cd src/belt.pp
user@pc:~/projects/publiq.pp/src/belt.pp$ git checkout master
user@pc:~/projects/publiq.pp/src/belt.pp$ git pull
user@pc:~/projects/publiq.pp/src/belt.pp$ cd ../..
user@pc:~/projects/publiq.pp$ cd src/mesh.pp
user@pc:~/projects/publiq.pp/src/mesh.pp$ cd src/belt.pp
user@pc:~/projects/publiq.pp/src/mesh.pp/src/belt.pp$ git checkout master
user@pc:~/projects/publiq.pp/src/mesh.pp/src/belt.pp$ git pull
user@pc:~/projects/publiq.pp/src/mesh.pp/src/belt.pp$ cd ../..
user@pc:~/projects/publiq.pp/src/mesh.pp$ git checkout master
user@pc:~/projects/publiq.pp/src/mesh.pp$ git pull
```
as you see, belt.pp appears twice as a submodule, the other one that relies under mesh.pp, does not participate in the build process, we just keep up with it, to have proper repository history

### how to use publiqd?
there is a command line arguments help, use it wisely! we have more details on the [wiki](https://github.com/publiqnet/publiq.pp/wiki/1.2-Executables "wiki") page.
also pay attention to all the console logging.
everything's is in an active development stage.

### more details?
refer to the [wiki](https://github.com/publiqnet/publiq.pp/wiki "wiki") page.
don't forget to check out mesh.pp and belt.pp readme files
