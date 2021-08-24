project(3pc)

include_directories(.)

add_library(threepc util.cpp)

add_subdirectory(agent)
add_subdirectory(broker)
add_subdirectory(directory)
add_subdirectory(runtime_locking_shard)
add_subdirectory(ticket_machine)
