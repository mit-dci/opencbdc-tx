project(ticket_machine)

add_library(ticket_machine impl.cpp
                           client.cpp
                           state_machine.cpp
                           controller.cpp)

add_executable(ticket_machined ticket_machined.cpp)
target_link_libraries(ticket_machined ticket_machine
                                      threepc
                                      rpc
                                      raft
                                      network
                                      serialization
                                      common
                                      crypto
                                      ${NURAFT_LIBRARY}
                                      ${LEVELDB_LIBRARY}
                                      ${CMAKE_THREAD_LIBS_INIT})
