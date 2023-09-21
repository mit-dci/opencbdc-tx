#!/bin/bash

echo "Installing build tools..."

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

if [[ "$OSTYPE" == "darwin"* ]]; then
  CPUS=$(sysctl -n hw.ncpu)
  # ensure development environment is set correctly for clang
  $SUDO xcode-select -switch /Library/Developer/CommandLineTools
  brew install llvm@14 googletest google-benchmark lcov make wget cmake curl
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

PYTHON_TIDY=/usr/local/bin/run-clang-tidy.py
if [ ! -f "${PYTHON_TIDY}" ]; then
  echo -e "${green}Copying run-clang-tidy to /usr/local/bin"
  wget https://raw.githubusercontent.com/llvm/llvm-project/e837ce2a32369b2e9e8e5d60270c072c7dd63827/clang-tools-extra/clang-tidy/tool/run-clang-tidy.py
  $SUDO mv run-clang-tidy.py /usr/local/bin
fi
