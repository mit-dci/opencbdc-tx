project(sentinel_2pc)

add_library(sentinel_2pc controller.cpp
                         server.cpp)

add_executable(sentineld-2pc sentineld_2pc.cpp)
target_link_libraries(sentineld-2pc sentinel_2pc
                                    sentinel
                                    sentinel_interface
                                    transaction
                                    coordinator
                                    raft
                                    rpc
                                    network
                                    crypto
                                    serialization
                                    common
                                    ${NURAFT_LIBRARY}
                                    secp256k1
                                    ${CMAKE_THREAD_LIBS_INIT})
