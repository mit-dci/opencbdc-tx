project(config_gen)

include_directories(../../src ../../config/tools ../../3rdparty/secp256k1/include)

add_library(config_generator config_generator.cpp)

add_executable(generate_config generate_configd.cpp)

target_link_libraries(generate_config config_generator
                                      util
                                      network
                                      common
                                      crypto
                                      secp256k1)
