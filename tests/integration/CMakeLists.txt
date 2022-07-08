project(integration)

add_executable(run_integration_tests mock_system.cpp
                                     atomizer_raft_integration_test.cpp
                                     atomizer_end_to_end_test.cpp
                                     sentinel_integration_test.cpp
                                     sentinel_2pc_integration_test.cpp
                                     shard_integration_test.cpp
                                     replicated_shard_integration_tests.cpp
                                     replicated_atomizer_integration_tests.cpp
                                     two_phase_end_to_end_test.cpp
                                     watchtower_integration_test.cpp)

target_compile_options(run_integration_tests PRIVATE -ftest-coverage -fprofile-arcs)
target_link_options(run_integration_tests PRIVATE --coverage)
target_link_libraries(run_integration_tests ${GTEST_LIBRARY}
                                            ${GTEST_MAIN_LIBRARY}
                                            util
                                            client
                                            atomizer_raft
                                            atomizer
                                            shard
                                            archiver
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
