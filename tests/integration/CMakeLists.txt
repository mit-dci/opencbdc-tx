project(integration)

add_executable(run_integration_tests smart_contract_atomic_commit_test.cpp
                                     mock_system.cpp
                                     atomizer_raft_integration_test.cpp
                                     atomizer_end_to_end_test.cpp
                                     gtest_evm_jsonrpc_client.cpp
                                     sample_erc20_contract.cpp
                                     sentinel_integration_test.cpp
                                     sentinel_2pc_integration_test.cpp
                                     shard_integration_test.cpp
                                     replicated_shard_integration_tests.cpp
                                     replicated_atomizer_integration_tests.cpp
                                     parsec_evm_end_to_end_test.cpp
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
                                            evm_runner
                                            lua_runner
                                            ticket_machine
                                            runtime_locking_shard
                                            runners
                                            agent
                                            broker
                                            directory
                                            json_rpc_http
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
                                            ${JSON_LIBRARY}
                                            ${LUA_LIBRARY}
                                            ${CURL_LIBRARY}
                                            ${MHD_LIBRARY}
                                            ${CMAKE_THREAD_LIBS_INIT})
