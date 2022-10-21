#!/bin/bash

echo "Configuring..."

echo "test local pipeline changes"

green="\033[0;32m"
cyan="\033[0;36m"
end="\033[0m"

set -e

SUDO=''
if (( $EUID != 0 )); then
    echo -e "non-root user, sudo required"
    SUDO='sudo'
fi

CMAKE_BUILD_TYPE="Debug"
if [[ "$BUILD_RELEASE" == "1" ]]; then
  CMAKE_BUILD_TYPE="Release"
fi

CPUS=1
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
  CPUS=$(grep -c ^processor /proc/cpuinfo)
elif [[ "$OSTYPE" == "darwin"* ]]; then
  CPUS=$(sysctl -n hw.ncpu)
  # ensure development environment is set correctly for clang
  $SUDO xcode-select -switch /Library/Developer/CommandLineTools
  brew install llvm@14 googletest google-benchmark lcov make wget cmake
  CLANG_TIDY=/usr/local/bin/clang-tidy
  if [ ! -L "$CLANG_TIDY" ]; then
    $SUDO ln -s $(brew --prefix)/opt/llvm@14/bin/clang-tidy /usr/local/bin/clang-tidy
  fi
  GMAKE=/usr/local/bin/gmake
  if [ ! -L "$GMAKE" ]; then
    $SUDO ln -s $(xcode-select -p)/usr/bin/gnumake /usr/local/bin/gmake
  fi
fi

if [[ "$OSTYPE" == "linux-gnu"* ]]; then
  apt update
  apt install -y build-essential wget cmake libgtest-dev libbenchmark-dev lcov git software-properties-common rsync unzip

  wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | $SUDO apt-key add -
  $SUDO add-apt-repository "deb http://apt.llvm.org/focal/ llvm-toolchain-focal-14 main"
  $SUDO apt install -y clang-format-14 clang-tidy-14
  $SUDO ln -s -f $(which clang-format-14) /usr/local/bin/clang-format
  $SUDO ln -s -f $(which clang-tidy-14) /usr/local/bin/clang-tidy
fi

LEVELDB_VERSION="1.23"
echo -e "${green}Building LevelDB from sources...${end}"
wget https://github.com/google/leveldb/archive/${LEVELDB_VERSION}.tar.gz
rm -rf "leveldb-${LEVELDB_VERSION}-${CMAKE_BUILD_TYPE}"
tar xzvf ${LEVELDB_VERSION}.tar.gz
rm -rf ${LEVELDB_VERSION}.tar.gz
mv leveldb-${LEVELDB_VERSION} "leveldb-${LEVELDB_VERSION}-${CMAKE_BUILD_TYPE}"
cd "leveldb-${LEVELDB_VERSION}-${CMAKE_BUILD_TYPE}"
cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DLEVELDB_BUILD_TESTS=0 -DLEVELDB_BUILD_BENCHMARKS=0 -DBUILD_SHARED_LIBS=0 -DHAVE_SNAPPY=0 .
make -j$CPUS
$SUDO make install
cd ..

NURAFT_VERSION="1.3.0"
echo -e "${green}Building NuRaft from sources...${end}"
wget https://github.com/eBay/NuRaft/archive/v${NURAFT_VERSION}.tar.gz
rm -rf "NuRaft-${NURAFT_VERSION}-${CMAKE_BUILD_TYPE}"
tar xzvf v${NURAFT_VERSION}.tar.gz
rm v${NURAFT_VERSION}.tar.gz
mv NuRaft-${NURAFT_VERSION} "NuRaft-${NURAFT_VERSION}-${CMAKE_BUILD_TYPE}"
cd "NuRaft-${NURAFT_VERSION}-${CMAKE_BUILD_TYPE}"
./prepare.sh
if [[ "$BUILD_RELEASE" == "1" ]]; then
  # If we're doing a release build, remove the examples and tests
  rm -rf examples tests
  mkdir examples
  mkdir tests
  touch examples/CMakeLists.txt
  touch tests/CMakeLists.txt
fi
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DDISABLE_SSL=1 ..
make -j$CPUS static_lib

echo -e "${green}Copying nuraft to /usr/local"
$SUDO cp libnuraft.a /usr/local/lib
$SUDO cp -r ../include/libnuraft /usr/local/include

cd ../..

PYTHON_TIDY=/usr/local/bin/run-clang-tidy.py
if [ ! -f "${PYTHON_TIDY}" ]; then
  echo -e "${green}Copying run-clang-tidy to /usr/local/bin"
  wget https://raw.githubusercontent.com/llvm/llvm-project/e837ce2a32369b2e9e8e5d60270c072c7dd63827/clang-tools-extra/clang-tidy/tool/run-clang-tidy.py
  $SUDO mv run-clang-tidy.py /usr/local/bin
fi

wget https://www.lua.org/ftp/lua-5.4.3.tar.gz
rm -rf lua-5.4.3
tar zxf lua-5.4.3.tar.gz
rm -rf lua-5.4.3.tar.gz
cd lua-5.4.3
make -j$CPUS
$SUDO make install
cd ..

CURL_VERSION="7.83.1"
wget https://curl.se/download/curl-${CURL_VERSION}.tar.gz
rm -rf curl-${CURL_VERSION}
tar xzvf curl-${CURL_VERSION}.tar.gz
rm -rf curl-${CURL_VERSION}.tar.gz
mkdir -p curl-${CURL_VERSION}/build
cd curl-${CURL_VERSION}/build
../configure --disable-shared --without-ssl --without-libpsl --without-libidn2 --without-brotli --without-zstd --without-zlib
make -j$CPUS
$SUDO make install
cd ../..

JSONCPP_VERSION="1.9.5"
wget https://github.com/open-source-parsers/jsoncpp/archive/refs/tags/${JSONCPP_VERSION}.tar.gz
rm -rf jsoncpp-${JSONCPP_VERSION}
tar xzvf ${JSONCPP_VERSION}.tar.gz
rm -rf ${JSONCPP_VERSION}.tar.gz
mkdir -p jsoncpp-${JSONCPP_VERSION}/build
cd jsoncpp-${JSONCPP_VERSION}/build
cmake .. -DBUILD_SHARED_LIBS=NO -DBUILD_STATIC_LIBS=YES -DJSONCPP_WITH_TESTS=OFF -DJSONCPP_WITH_POST_BUILD_UNITTEST=OFF
make -j$CPUS
$SUDO make install
cd ../..

wget https://github.com/ethereum/evmc/archive/eda05c6866ac06bd93d62b605cbec5839d85c221.zip
rm -rf evmc-eda05c6866ac06bd93d62b605cbec5839d85c221
unzip eda05c6866ac06bd93d62b605cbec5839d85c221.zip
rm eda05c6866ac06bd93d62b605cbec5839d85c221.zip
cd evmc-eda05c6866ac06bd93d62b605cbec5839d85c221
mkdir build
cd build
cmake ..
make -j$CPUS
$SUDO make install
cd ../..

wget https://github.com/ethereum/evmone/archive/be870917e8cefd2b125bd27375dd9d2409ff1f68.zip
rm -rf evmone-be870917e8cefd2b125bd27375dd9d2409ff1f68
unzip be870917e8cefd2b125bd27375dd9d2409ff1f68.zip
rm be870917e8cefd2b125bd27375dd9d2409ff1f68.zip
cd evmone-be870917e8cefd2b125bd27375dd9d2409ff1f68
rm -rf evmc
mv ../evmc-eda05c6866ac06bd93d62b605cbec5839d85c221 ./evmc
mkdir ./evmc/.git
cmake -S . -B build -DBUILD_SHARED_LIBS=OFF
cmake --build build --parallel
cd build
$SUDO make install
cd ../..
rm -rf evmone-be870917e8cefd2b125bd27375dd9d2409ff1f68

wget https://github.com/chfast/ethash/archive/e3e002ecc25ca699349aa62fa38e7b7cc5f653af.zip
rm -rf ethash-e3e002ecc25ca699349aa62fa38e7b7cc5f653af
unzip e3e002ecc25ca699349aa62fa38e7b7cc5f653af.zip
rm e3e002ecc25ca699349aa62fa38e7b7cc5f653af.zip
cd ethash-e3e002ecc25ca699349aa62fa38e7b7cc5f653af
mkdir build
cd build
cmake -DETHASH_BUILD_ETHASH=OFF -DETHASH_BUILD_TESTS=OFF ..
cmake --build . --parallel
$SUDO cp ./lib/keccak/libkeccak.a /usr/local/lib
$SUDO cp -r ../include/ethash /usr/local/include
cd ../..

wget https://gnu.askapache.com/libmicrohttpd/libmicrohttpd-0.9.75.tar.gz
rm -rf libmicrohttpd-0.9.75
tar xzvf libmicrohttpd-0.9.75.tar.gz
rm libmicrohttpd-0.9.75.tar.gz
cd libmicrohttpd-0.9.75
mkdir build
cd build
../configure --disable-curl --disable-examples --disable-doc --disable-shared --disable-https
make -j $CPUS
$SUDO make install
cd ../../
rm -rf libmicrohttpd-0.9.75
