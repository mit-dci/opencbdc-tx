project(3pc)

include_directories(.)

add_library(threepc util.cpp)

add_subdirectory(ticket_machine)
