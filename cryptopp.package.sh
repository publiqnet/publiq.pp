git clone --depth=1 https://github.com/weidai11/cryptopp
cd cryptopp
make static
make install -j 8 PREFIX=../install
cd ..
rm -rf cryptopp


