#!/bin/bash
set -e

echo "Building..."

if [ -z ${BUILD_DIR+x} ]; then
    export BUILD_DIR=build
fi

mkdir -p $BUILD_DIR
cd $BUILD_DIR

CMAKE_FLAGS=""
CPUS=1
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    CPUS=$(grep -c ^processor /proc/cpuinfo)
elif [[ "$OSTYPE" == "darwin"* ]]; then
    CPUS=$(sysctl -n hw.ncpu)
    XCODE_CMDLINE_DIR=$(xcode-select -p)
    CMAKE_FLAGS+="-DCMAKE_C_COMPILER=${XCODE_CMDLINE_DIR}/usr/bin/clang -DCMAKE_CXX_COMPILER=${XCODE_CMDLINE_DIR}/usr/bin/clang++ -DCMAKE_CXX_FLAGS=-isystem\ /usr/local/include -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
fi

CMAKE_BUILD_TYPE="Debug"
if [[ "$BUILD_RELEASE" == "1" ]]; then
    CMAKE_BUILD_TYPE="Release"
elif [[ "$BUILD_PROFILING" == "1" ]]; then
    CMAKE_BUILD_TYPE="Profiling"
fi

export LD_LIBRARY_PATH=${PWD}/../src/util/oracle/instantclient:${LD_LIBRARY_PATH}
echo "LD_LIBRARY_PATH=${LD_LIBRARY_PATH}"
eval "cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} ${CMAKE_FLAGS} .."
make -j$CPUS

# copy instantclient-basic-linux.x64-21.11.0.0.0dbru.zip and sdk from src/util/oracle to build/src/util/oracle
cp ../src/util/oracle/instantclient-basic-linux.x64-21.11.0.0.0dbru.zip ./build/src/util/oracle/instantclient-basic.zip
cp ../src/util/oracle/instantclient-sdk-linux.x64-21.11.0.0.0dbru.zip ./build/src/util/oracle/instantclient-sdk.zip
# unzip both
unzip ./build/src/util/oracle/instantclient-basic.zip -d ./build/src/util/oracle
unzip ./build/src/util/oracle/instantclient-sdk.zip -d ./build/src/util/oracle
mv instantclient_21_11 instantclient

# mkdir -p ./build/src/util/oracle/instantclient
# cp -r ../src/util/oracle/instantclient ./build/src/util/oracle/instantclient
cp ../src/util/oracle/key.txt ./build/src/util/oracle/key.txt
