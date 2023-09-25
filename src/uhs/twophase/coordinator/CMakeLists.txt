project(coordinator)

include_directories(../../util/oracle)

add_library(coordinator format.cpp
                        state_machine.cpp
                        client.cpp
                        distributed_tx.cpp
                        controller.cpp
                        server.cpp)

add_executable(coordinatord coordinatord.cpp)
target_link_libraries(coordinatord coordinator
                                   locking_shard
                                   raft
                                   transaction
                                   rpc
                                   network
                                   common
                                   serialization
                                   crypto
                                   ${NURAFT_LIBRARY}
                                   ${LEVELDB_LIBRARY}
                                   secp256k1
                                   ${CMAKE_THREAD_LIBS_INIT}
                                   oracle)
