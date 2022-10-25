project(archiver)

add_library(archiver client.cpp
                     controller.cpp)

add_executable(archiverd archiverd.cpp)
target_link_libraries(archiverd archiver
                                atomizer
                                watchtower
                                transaction
                                network
                                common
                                serialization
                                crypto
                                ${LEVELDB_LIBRARY}
                                ${NURAFT_LIBRARY}
                                secp256k1
                                ${CMAKE_THREAD_LIBS_INIT})
