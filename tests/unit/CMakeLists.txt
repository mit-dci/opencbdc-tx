project(unit)

add_executable(run_unit_tests archiver_test.cpp
                              atomizer/messages_test.cpp
                              atomizer_test.cpp
                              buffer_test.cpp
                              common/hash_test.cpp
                              config_test.cpp
                              coordinator/messages_test.cpp
                              locking_shard/format_test.cpp
                              locking_shard/controller_test.cpp
                              coordinator/controller_test.cpp
                              network_test.cpp
                              message_test.cpp
                              raft_test.cpp
                              rpc/tcp_test.cpp
                              sentinel_2pc/controller_test.cpp
                              serialization_test.cpp
                              serialization/format_test.cpp
                              shard_test.cpp
                              socket_test.cpp
                              serialization/stream_serializer_test.cpp
                              transaction_test.cpp
                              twophase_test.cpp
                              validation_test.cpp
                              wallet_test.cpp
                              watchtower/block_cache_test.cpp
                              watchtower/error_cache_test.cpp
                              watchtower/tx_error_messages_test.cpp
                              watchtower/watchtower_test.cpp)

target_compile_options(run_unit_tests PRIVATE -ftest-coverage -fprofile-arcs)
target_link_options(run_unit_tests PRIVATE --coverage)
target_link_libraries(run_unit_tests ${GTEST_LIBRARY}
                                     ${GTEST_MAIN_LIBRARY}
                                     util
                                     client
                                     atomizer
                                     atomizer_raft
                                     archiver
                                     shard
                                     sentinel
                                     sentinel_2pc
                                     sentinel_interface
                                     watchtower
                                     coordinator
                                     locking_shard
                                     raft
                                     rpc
                                     transaction
                                     network
                                     common
                                     serialization
                                     crypto
                                     secp256k1
                                     ${LEVELDB_LIBRARY}
                                     ${NURAFT_LIBRARY}
                                     ${CMAKE_THREAD_LIBS_INIT})
