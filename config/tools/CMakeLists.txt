project(config_gen)

include_directories(../../src ../../3rdparty/secp256k1/include)

set(SECP256K1_LIBRARY $<TARGET_FILE:secp256k1>)

add_executable(generate_config config_generator.cpp)

target_link_libraries(generate_config util
                                      network
                                      common
                                      crypto
                                      secp256k1)
