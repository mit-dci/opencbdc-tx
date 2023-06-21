project(runtime_locking_shard)

add_library(runtime_locking_shard impl.cpp
                                  client.cpp
                                  format.cpp
                                  state_machine.cpp
                                  controller.cpp
                                  replicated_shard.cpp
                                  server.cpp
                                  replicated_shard_client.cpp)

add_executable(runtime_locking_shardd runtime_locking_shardd.cpp)
target_link_libraries(runtime_locking_shardd runtime_locking_shard
                                             parsec
                                             raft
                                             rpc
                                             network
                                             serialization
                                             common
                                             crypto
                                             ${NURAFT_LIBRARY}
                                             ${LEVELDB_LIBRARY}
                                             ${CMAKE_THREAD_LIBS_INIT})
