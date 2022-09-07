project(atomizer)

add_library(atomizer atomizer.cpp
                     block.cpp
                     state_machine.cpp
                     format.cpp
                     messages.cpp)

add_library(atomizer_raft atomizer_raft.cpp
                          controller.cpp
                          state_machine.cpp)

add_executable(atomizer-raftd atomizer-raftd.cpp)
target_link_libraries(atomizer-raftd atomizer_raft
                                     atomizer
                                     raft
                                     watchtower
                                     transaction
                                     network
                                     common
                                     serialization
                                     crypto
                                     ${NURAFT_LIBRARY}
                                     ${LEVELDB_LIBRARY}
                                     secp256k1
                                     ${CMAKE_THREAD_LIBS_INIT}
                                     snappy)
