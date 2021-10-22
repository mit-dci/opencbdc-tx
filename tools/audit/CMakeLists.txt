project(audit)

include_directories(../../src)

add_executable(audit audit.cpp)
target_link_libraries(audit common
                            crypto)
