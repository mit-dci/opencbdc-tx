project(audit)

include_directories(../../src)

add_executable(audit audit.cpp)
target_link_libraries(audit transaction
                            common
                            util
                            serialization
                            crypto
                            secp256k1)
