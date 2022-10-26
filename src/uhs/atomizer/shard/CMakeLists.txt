project(shard)

add_library(shard shard.cpp
                  controller.cpp)

add_executable(shardd shardd.cpp)
target_link_libraries(shardd shard
                             atomizer
                             archiver
                             watchtower
                             transaction
                             network
                             common
                             serialization
                             crypto
                             ${LEVELDB_LIBRARY}
                             ${NURAFT_LIBRARY}
                             secp256k1
                             ${CMAKE_THREAD_LIBS_INIT}
                             ${SNAPPY_LIBRARY})
