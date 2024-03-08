project(benchmarks)

include_directories(. ../src ../tools/watchtower ../3rdparty ../3rdparty/secp256k1/include)
set(SECP256K1_LIBRARY $<TARGET_FILE:secp256k1>)

add_executable(run_benchmarks   low_level.cpp
                                transactions.cpp
                                uhs_leveldb.cpp
                                uhs_set.cpp
                                )

target_compile_options(run_benchmarks PRIVATE -ftest-coverage -fprofile-arcs)
target_link_options(run_benchmarks PRIVATE --coverage)
target_link_libraries(run_benchmarks ${GTEST_LIBRARY}
                                     ${GTEST_MAIN_LIBRARY}
                                     benchmark::benchmark
                                     util
                                     shard
                                     watchtower
                                     locking_shard
                                     transaction
                                     rpc
                                     network
                                     common
                                     serialization
                                     crypto
                                     secp256k1
                                     ${LEVELDB_LIBRARY}
                                     ${CMAKE_THREAD_LIBS_INIT})
