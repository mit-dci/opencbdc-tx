project(agent)

add_library(agent impl.cpp
                  interface.cpp
                  server_interface.cpp
                  client.cpp
                  format.cpp)

add_subdirectory(runners)

add_executable(agentd agentd.cpp)
target_link_libraries(agentd broker
                             directory
                             ticket_machine
                             runtime_locking_shard
                             parsec
                             evm_runner
                             lua_runner
                             py_runner
                             agent
                             json_rpc_http
                             runners
                             rpc
                             network
                             serialization
                             common
                             crypto
                             secp256k1
                             ${LUA_LIBRARY}
                             ${JSON_LIBRARY}
                             ${MHD_LIBRARY}
                             ${CMAKE_THREAD_LIBS_INIT})
