project(bech32)

add_library(bech32 bech32.cpp)
target_include_directories(bech32 PUBLIC "." "./util")
