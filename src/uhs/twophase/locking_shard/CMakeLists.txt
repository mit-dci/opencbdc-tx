project(locking_shard)

add_library(locking_shard client.cpp
                          controller.cpp
                          locking_shard.cpp
                          interface.cpp
                          format.cpp
                          messages.cpp
                          state_machine.cpp
                          status_client.cpp
                          status_server.cpp)

add_executable(locking-shardd locking_shardd.cpp)
target_link_libraries(locking-shardd locking_shard
                                     raft
                                     transaction
                                     rpc
                                     network
                                     common
                                     serialization
                                     crypto
                                     secp256k1
                                     ${NURAFT_LIBRARY}
                                     ${LEVELDB_LIBRARY}
                                     ${CMAKE_THREAD_LIBS_INIT}
                                     snappy)
