add_executable(lua_bench lua_bench.cpp
                         wallet.cpp)
add_executable(emplace_contract emplace_lua_contract.cpp)
target_link_libraries(lua_bench broker
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
                                ${LUA_LIBRARY}
                                ${CMAKE_THREAD_LIBS_INIT})

target_link_libraries(emplace_contract broker
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
                                ${LUA_LIBRARY}
                                ${CMAKE_THREAD_LIBS_INIT})
