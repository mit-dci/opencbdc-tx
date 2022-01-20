project(client)

add_library(client atomizer_client.cpp
                   client.cpp
                   twophase_client.cpp)

add_executable(client-cli client-cli.cpp)
target_link_libraries(client-cli client
                                 atomizer
                                 bech32
                                 atomizer
                                 watchtower
                                 sentinel
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
