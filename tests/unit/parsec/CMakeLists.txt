add_library(parsec_unit_tests util.cpp)

add_subdirectory(runtime_locking_shard)
add_subdirectory(broker)
add_subdirectory(agent)
