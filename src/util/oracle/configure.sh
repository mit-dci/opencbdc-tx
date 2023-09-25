#!/bin/bash

# remove build directory
remake() {
    if [ -d "build" ]; then
        echo '---------------------------------'
        echo 'CLEANING BUILD DIRECTORY'
        echo '---------------------------------'
        rm -rf build

        echo '---------------------------------'
        echo 'RUNNING CMAKE ON ORACLEDB FILE'
        echo '---------------------------------'
        mkdir build
        cd build
        cmake ..
        echo '---------------------------------'
        echo 'CMAKE BUILD COMPLETE'
        echo '---------------------------------'
        echo ''

        echo '---------------------------------'
        echo 'RUNNING MAKE ON ORACLEDB FILE'
        echo '---------------------------------'
        make
        echo '---------------------------------'
        echo 'MAKE BUILD COMPLETE'
        echo '---------------------------------'
        echo ''
        cd ..
    fi
    exit 0
}

# remove build directory and instantclient directory
clean() {
    if [ -d "build" ]; then
        echo '---------------------------------'
        echo 'CLEANING BUILD DIRECTORY'
        echo '---------------------------------'
        rm -rf build

        echo '---------------------------------'
        echo 'CLEANING INSTANT CLIENT DIRECTORY'
        echo '---------------------------------'
        rm -rf instantclient
        echo ''
    fi
    exit 0
}

if [ "$1" == "--remake" ]; then
    remake
fi

if [ "$1" == "--clean" ]; then
    clean
fi

# check if libaio1 and libaio-dev are installed
if ! [ -x "$(command -v libaio1)" ]; then
    echo '---------------------------------'
    echo 'INSTALLING LIBAIO1'
    echo '---------------------------------'
    sudo apt-get update
    sudo apt-get install -y libaio1
    echo '---------------------------------'
    echo 'LIBAIO1 INSTALLED'
    echo '---------------------------------'
    echo ''
fi

if ! [ -x "$(command -v libaio-dev)" ]; then
    echo '---------------------------------'
    echo 'INSTALLING LIBAIO-DEV'
    echo '---------------------------------'
    sudo apt-get update
    sudo apt-get install -y libaio-dev
    echo '---------------------------------'
    echo 'LIBAIO-DEV INSTALLED'
    echo '---------------------------------'
    echo ''
fi

# check if wget is installed
if ! [ -x "$(command -v wget)" ]; then
    echo '---------------------------------'
    echo 'INSTALLING WGET'
    echo '---------------------------------'
    sudo apt-get install -y wget
    echo '---------------------------------'
    echo 'WGET INSTALLED'
    echo '---------------------------------'
    echo ''
fi

# check if instantclinet is already installed
if [ -d "instantclient" ]; then
    echo '---------------------------------'
    echo 'INSTANT CLIENT ALREADY INSTALLED'
    echo '---------------------------------'
    echo ''
    exit 0
fi

# wget instantclient-basic-linux.x64-21.11
echo '---------------------------------'
echo 'DOWNLOADING ORACLE INSTANT CLIENT'
echo '---------------------------------'
wget -c https://download.oracle.com/otn_software/linux/instantclient/2111000/instantclient-basic-linux.x64-21.11.0.0.0dbru.zip
# wget instantclient-sdk-linux.x64-21.11
wget -c https://download.oracle.com/otn_software/linux/instantclient/2111000/instantclient-sdk-linux.x64-21.11.0.0.0dbru.zip
echo '---------------------------------'
echo 'INSTANT CLIENT DOWNLOAD COMPLETE'
echo '---------------------------------'
echo ''

echo '---------------------------------'
echo 'UNZIPPING ORACLE INSTANT CLIENT'
echo '---------------------------------'
# unzip instantclient-basic-linux.x64-21.11
unzip instantclient-basic-linux.x64-21.11.0.0.0dbru.zip
# unzip instantclient-sdk-linux.x64-21.11
unzip instantclient-sdk-linux.x64-21.11.0.0.0dbru.zip
echo '---------------------------------'
echo 'INSTANT CLIENT UNZIP COMPLETE'
echo '---------------------------------'
echo ''

# rename instantclient_21_11 to instantclient
mv instantclient_21_11 instantclient

# export LD_LIBRARY_PATH
PWD=$(pwd)
export LD_LIBRARY_PATH=$PWD/instantclient:$LD_LIBRARY_PATH

# export PATH
export PATH=$PWD/instantclient:$PATH

# delete zip files (no longer needed)
rm instantclient-basic-linux.x64-21.11.0.0.0dbru.zip
rm instantclient-sdk-linux.x64-21.11.0.0.0dbru.zip

# unzip wallet file, change sqlnet.ora file to have ../wallet
# make changes to sqlnet.ora file

echo '---------------------------------'
echo 'RUNNING CMAKE ON ORACLEDB FILE'
echo '---------------------------------'
mkdir build
cd build
cmake ..
echo '---------------------------------'
echo 'CMAKE BUILD COMPLETE'
echo '---------------------------------'
echo ''

echo '---------------------------------'
echo 'RUNNING MAKE ON ORACLEDB FILE'
echo '---------------------------------'
make
echo '---------------------------------'
echo 'MAKE BUILD COMPLETE'
echo '---------------------------------'
echo ''
cd ..