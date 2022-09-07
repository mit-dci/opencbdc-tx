#!/bin/bash

echo "Configuring..."

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
  brew install leveldb llvm@11 googletest lcov make wget cmake
  CLANG_TIDY=/usr/local/bin/clang-tidy
  if [ ! -L "$CLANG_TIDY" ]; then
    $SUDO ln -s $(brew --prefix)/opt/llvm@11/bin/clang-tidy /usr/local/bin/clang-tidy
  fi
  GMAKE=/usr/local/bin/gmake
  if [ ! -L "$GMAKE" ]; then
    $SUDO ln -s $(xcode-select -p)/usr/bin/gnumake /usr/local/bin/gmake
  fi
fi

if [[ "$OSTYPE" == "linux-gnu"* ]]; then
  if [ -f "/etc/arch-release" ]; then
    pacman -Syu
    pacman -S wget cmake gtest lcov git rsync gcc make snappy
  else
    apt update
    apt install -y build-essential wget cmake libgtest-dev lcov git software-properties-common rsync
  fi

  # GitHub Actions in .github/workflows/validation.yml will attempt to cache and reuse leveldb built in this block.
  # If a folder called leveldb-1.22 exists, skip the build step and go straight to install.
  # See https://docs.github.com/en/free-pro-team@latest/actions/guides/caching-dependencies-to-speed-up-workflows
  if [ ! -d "leveldb-1.22-${CMAKE_BUILD_TYPE}" ]; then
    echo -e "${green}Building LevelDB from sources...${end}"
    wget https://github.com/google/leveldb/archive/1.22.tar.gz
    tar xzvf 1.22.tar.gz
    rm -rf 1.22.tar.gz
    mv leveldb-1.22 "leveldb-1.22-${CMAKE_BUILD_TYPE}"
    cd "leveldb-1.22-${CMAKE_BUILD_TYPE}"
    eval "cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DLEVELDB_BUILD_TESTS=0 -DLEVELDB_BUILD_BENCHMARKS=0 -DBUILD_SHARED_LIBS=0 ."
    make -j$CPUS
  else
    echo -e "${green}Installing LevelDB from cache...${end}"
    cd "leveldb-1.22-${CMAKE_BUILD_TYPE}"
  fi
  make install
  cd ..

  if [ -f "/etc/arch-release" ]; then
    pacman -S llvm13
  else
    wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -
    add-apt-repository "deb http://apt.llvm.org/focal/ llvm-toolchain-focal-13 main"
    apt install -y clang-format-13 clang-tidy-13
  fi
  
  ln -s -f $(which clang-format-13) /usr/local/bin/clang-format
  ln -s -f $(which clang-tidy-13) /usr/local/bin/clang-tidy
fi

NURAFT_VERSION="1.3.0"
if [ ! -d "NuRaft-${NURAFT_VERSION}-${CMAKE_BUILD_TYPE}" ]; then
  echo -e "${green}Building NuRaft from sources...${end}"
  wget https://github.com/eBay/NuRaft/archive/v${NURAFT_VERSION}.tar.gz
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
  eval "cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DDISABLE_SSL=1 .."
  eval "make -j$CPUS static_lib"
else
  echo -e "${green}Installing NuRaft from cache...${end}"
  cd "NuRaft-${NURAFT_VERSION}-${CMAKE_BUILD_TYPE}/build"
fi

echo -e "${green}Copying nuraft to /usr/local"
$SUDO cp libnuraft.a /usr/local/lib
$SUDO cp -r ../include/libnuraft /usr/local/include

cd ..

PYTHON_TIDY=/usr/local/bin/run-clang-tidy.py
if [ ! -f "${PYTHON_TIDY}" ]; then
  echo -e "${green}Copying run-clang-tidy to /usr/local/bin"
  wget https://raw.githubusercontent.com/llvm/llvm-project/e837ce2a32369b2e9e8e5d60270c072c7dd63827/clang-tools-extra/clang-tidy/tool/run-clang-tidy.py
  $SUDO mv run-clang-tidy.py /usr/local/bin
fi
