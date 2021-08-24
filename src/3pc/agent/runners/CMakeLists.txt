project(runners)

add_library(runners interface.cpp)

add_subdirectory(evm)
add_subdirectory(lua)
