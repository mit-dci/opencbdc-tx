project(sentinel)

add_library(sentinel controller.cpp
                     server.cpp)

add_executable(sentineld sentineld.cpp)
target_link_libraries(sentineld sentinel
                                sentinel_interface
                                transaction
                                rpc
                                network
                                common
                                serialization
                                crypto
                                secp256k1
                                ${CMAKE_THREAD_LIBS_INIT})
