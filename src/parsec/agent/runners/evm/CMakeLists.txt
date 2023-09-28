project(evm_runner)

add_library(evm_runner address.cpp
                       impl.cpp
                       math.cpp
                       hash.cpp
                       host.cpp
                       format.cpp
                       rlp.cpp
                       rlp_reader.cpp
                       rlp_writer.cpp
                       serialization.cpp
                       signature.cpp
                       util.cpp
                       http_server.cpp)

target_link_libraries(evm_runner parsec
                                 ${KECCAK_LIBRARY}
                                 ${EVMC_HEX_LIBRARY}
                                 ${EVMONE_LIBRARY}
                                 ${EVMC_INSTRUCTIONS_LIBRARY})
