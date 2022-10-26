project(shard-seeder)

find_package(Threads)

include_directories(../../src ../../3rdparty ../../3rdparty/secp256k1/include)

add_executable(shard-seeder shard-seeder.cpp)
target_link_libraries(shard-seeder transaction
                                   network
                                   common
                                   serialization
                                   crypto
                                   ${LEVELDB_LIBRARY}
                                   ${CMAKE_THREAD_LIBS_INIT}
                                   secp256k1
                                   ${SNAPPY_LIBRARY})
