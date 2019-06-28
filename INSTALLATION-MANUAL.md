# Install PUBLIQ on Linux

In order to cover wide variety of cases this tutorial does some redundant steps, such as building manually cmake, boost and cryptopp. Feel free to experiment for more optimal results.

Run the following as root
```console
[root@localhost ~]# yum install gcc gcc-c++ make glibc-devel git cmake boost boost-locale boost-filesystem boost-program-options boost-system boost-devel wget
```
or in case of apt-get
```console
[root@localhost ~]# apt-get install build-essential libboost-all-dev libcrypto++-dev git cmake
```

I read that the following would install cryptopp library, on RHEL
```console
[root@localhost ~]# yum install cryptopp cryptopp-devel
```
but it failed for me, so we'll have to build it manually below.

Exit the root console, from now on, just the simple user is needed

RHEL has the philosophy to stick to aging software and *cmake* that comes with it is old, we'll build a fresh one now

```console
[user@localhost ~]$ wget https://cmake.org/files/v3.7/cmake-3.7.2.tar.gz
[user@localhost ~]$ tar xf cmake-3.7.2.tar.gz
[user@localhost ~]$ mkdir cmake-3.7.2_prefix
[user@localhost ~]$ cd cmake-3.7.2
[user@localhost ~]$ CMAKE_ROOT=$(realpath ../cmake-3.7.2_prefix)
[user@localhost ~]$ ./configure --prefix=$CMAKE_ROOT
[user@localhost ~]$ make install
[user@localhost ~]$ cd ..
[user@localhost ~]$ rm -rf cmake-3.7.2 cmake-3.7.2.tar.gz
[user@localhost ~]$ export PATH=$CMAKE_ROOT/bin:$PATH
```

Download the PUBLIQ source code, manually build cryptopp and boost, because the boost version that comes with RHEL is old too.
Then build the PUBLIQ.

```console
[user@localhost ~]$ git clone --recursive https://github.com/publiqnet/publiq.pp
[user@localhost ~]$ mkdir publiq
[user@localhost ~]$ cd publiq
[user@localhost publiq]$ ../publiq.pp/cryptopp.package.sh
[user@localhost publiq]$ ../publiq.pp/boost.package.sh
[user@localhost publiq]$ ../publiq.pp/publiq.pp.package.sh
```

Done, now there is ~/publiq/install/bin/publiq.pp/publiqd executable ready to use

Make sure before running publiqd executable  you have a ~/.config directory.

