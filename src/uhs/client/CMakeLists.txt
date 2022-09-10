project(client)

add_library(client atomizer_client.cpp
                   client.cpp
                   twophase_client.cpp)

target_link_libraries(client bech32)

add_executable(client-cli client-cli.cpp)
target_link_libraries(client-cli client
                                 atomizer
                                 watchtower
                                 sentinel
                                 sentinel_interface
                                 transaction
                                 locking_shard
                                 coordinator
                                 rpc
                                 network
                                 common
                                 serialization
                                 crypto
                                 secp256k1
                                 ${NURAFT_LIBRARY}
                                 ${CMAKE_THREAD_LIBS_INIT})
