add_executable(py_bench wallet.cpp py_bench.cpp)
target_link_libraries(py_bench  broker
                                directory
                                runtime_locking_shard
                                ticket_machine
                                parsec
                                agent
                                rpc
                                network
                                common
                                serialization
                                crypto
                                secp256k1
                                ${PYTHON_VERSION}
                                py_runner
                                ${CMAKE_THREAD_LIBS_INIT})

include_directories(/usr/include/${PYTHON_VERSION})
