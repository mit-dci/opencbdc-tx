cmake_minimum_required(VERSION 3.10)
include(CheckCXXCompilerFlag)

project(opencbdc-tx)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS False)

try_compile(USE_STDFS_LIB ${CMAKE_BINARY_DIR} "${CMAKE_SOURCE_DIR}/cmake-tests/test-libstdc++fs.cpp" LINK_LIBRARIES "stdc++fs")
check_cxx_compiler_flag(-Wshadow-all W_SHADOW_ALL)
check_cxx_compiler_flag(-Wnewline-eof W_NEWLINE_EOF)

set(CMAKE_CXX_OUTPUT_EXTENSION_REPLACE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads)

if(USE_STDFS_LIB)
    link_libraries(-lstdc++fs)
endif()

find_library(LEVELDB_LIBRARY leveldb REQUIRED)
find_library(NURAFT_LIBRARY nuraft REQUIRED)

message(STATUS "Checking if host system is Apple macOS")
if (CMAKE_HOST_APPLE)
 message(STATUS "macOS detected... Disabling Run Time Type Information (RTTI) features.")
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

include_directories(3rdparty 3rdparty/secp256k1/include)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-fprofile-arcs -ftest-coverage)
endif()


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_link_options(--coverage)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Profiling")
    add_link_options(-Wl,--build-id)
    add_compile_options(-O3
                        -fno-omit-frame-pointer
                        -g)
endif()

add_subdirectory(3rdparty)

add_compile_options(-Wall
                    -Wextra
                    -pedantic
                    -Werror
                    -fno-exceptions)

add_compile_definitions(_NO_EXCEPTION)

if(W_SHADOW_ALL)
    add_compile_options(-Wshadow-all)
else()
    add_compile_options(-Wshadow)
endif()

if(W_NEWLINE_EOF)
    add_compile_options(-Wnewline-eof)
endif()

add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(tools/bench)
add_subdirectory(tools/shard-seeder)
