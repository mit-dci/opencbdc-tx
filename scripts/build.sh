#!/bin/bash

set -e

help() {
    if [ $# -gt 0 ]; then
        printf 'Unexpected Argument (%s)\n' "$1"
    fi
    printf 'HELP: Usage: %s [Debug|Release|Profiling]\n' "$0"
    exit 0
}

# Note:
# CMAKE_BUILD_TYPE="Debug" adds "-O0 -g" flags by default
# CMAKE_BUILD_TYPE="Release" adds "-O3 -DNDEBUG" by default
if [[ "$BUILD_DEBUG" == "1" ]]; then
    CMAKE_BUILD_TYPE="Debug"
elif [[ "$BUILD_RELEASE" == "1" ]]; then
    CMAKE_BUILD_TYPE="Release"
elif [[ "$BUILD_PROFILING" == "1" ]]; then
    CMAKE_BUILD_TYPE="Profiling"
fi

if [ $# -gt 0 ]; then
    case "$1" in
        Release|--release|-r) CMAKE_BUILD_TYPE="Release";;
        Profiling|--profiling|-p) CMAKE_BUILD_TYPE="Profiling";;
        Debug|--debug|-d) CMAKE_BUILD_TYPE="Debug";;
        --help|-h) help;;
        *) help $1;;
    esac
fi

echo "Building..."

# see PREFIX in ./scripts/configure.sh
PREFIX="$(cd "$(dirname "$0")"/.. && pwd)/prefix"

if [ -z ${BUILD_DIR+x} ]; then
    export BUILD_DIR=build
fi

mkdir -p $BUILD_DIR
cd $BUILD_DIR

CMAKE_FLAGS=-DCMAKE_PREFIX_PATH="${PREFIX}"
CPUS=1
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    CPUS=$(grep -c ^processor /proc/cpuinfo)
elif [[ "$OSTYPE" == "darwin"* ]]; then
    CPUS=$(sysctl -n hw.ncpu)
    XCODE_CMDLINE_DIR=$(xcode-select -p)
    CMAKE_FLAGS+=" -DCMAKE_C_COMPILER=${XCODE_CMDLINE_DIR}/usr/bin/clang -DCMAKE_CXX_COMPILER=${XCODE_CMDLINE_DIR}/usr/bin/clang++ -DCMAKE_CXX_FLAGS=-isystem\ /usr/local/include -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
fi

if [[ -z $CMAKE_BUILD_TYPE ]]; then
    echo "CMAKE_BUILD_TYPE not set, defaulting to debug"
    CMAKE_BUILD_TYPE="Debug"
fi

echo "Building $CMAKE_BUILD_TYPE"
eval "cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} ${CMAKE_FLAGS} .."
make -j$CPUS

