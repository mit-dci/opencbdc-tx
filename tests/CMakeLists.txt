project(tests)

include_directories(. ../src ../tools/watchtower ../3rdparty ../3rdparty/secp256k1/include)
set(SECP256K1_LIBRARY $<TARGET_FILE:secp256k1>)

add_library(util util.cpp)

add_subdirectory(integration)
add_subdirectory(unit)
