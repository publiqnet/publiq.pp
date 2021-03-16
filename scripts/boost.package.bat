REM powershell -command "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls"
powershell -command "&{Write-Host \"downloading boost\"; (New-Object System.Net.WebClient).DownloadFile(\"http://dl.bintray.com/boostorg/release/1.70.0/source/boost_1_70_0.zip\", \"boost_1_70_0.zip\")}"

echo extracting zip
cmake -E tar xf "boost_1_70_0.zip" --format=zip"
del boost_1_70_0.zip
cd boost_1_70_0
call bootstrap.bat

call b2 install --reconfigure -j 8 --ignore-site-config --address-model=64 link=static --with-program_options --with-system --with-filesystem --with-locale --prefix=../install --build-dir=../boost-build
cd ..
rmdir /Q /S boost_1_70_0
rmdir /Q /S boost-build