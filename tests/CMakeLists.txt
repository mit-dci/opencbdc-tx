project(tests)

find_package(GTest REQUIRED)
include(../3rdparty/FindGMock.cmake)
find_package(Threads)
include_directories(. ${GTEST_INCLUDE_DIRS} ${GMOCK_INCLUDE_DIRS} ../src ../tools/watchtower ../3rdparty ../3rdparty/secp256k1/include)
set(SECP256K1_LIBRARY $<TARGET_FILE:secp256k1>)

add_library(util util.cpp)

add_subdirectory(integration)
add_subdirectory(unit)
