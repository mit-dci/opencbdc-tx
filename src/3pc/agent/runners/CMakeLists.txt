project(runners)

add_library(runners interface.cpp)

add_subdirectory(lua)
add_subdirectory(evm)
