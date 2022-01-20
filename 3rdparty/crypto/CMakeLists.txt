project(crypto)

add_library(crypto sha256_avx2.cpp
                   sha256_shani.cpp
                   sha256_sse4.cpp
                   sha256_sse41.cpp
                   sha256.cpp
                   siphash.cpp)
target_include_directories(crypto PRIVATE ".." ".")
set_source_files_properties(sha256_avx2.cpp PROPERTIES COMPILE_FLAGS
                                            "-DENABLE_AVX2 -mavx -mavx2")
set_source_files_properties(sha256_shani.cpp PROPERTIES COMPILE_FLAGS
                                             "-DENABLE_SHANI -msse4 -msha")
set_source_files_properties(sha256_sse41.cpp PROPERTIES COMPILE_FLAGS
                                             "-DENABLE_SSE41 -msse4.1")
set_source_files_properties(sha256.cpp PROPERTIES COMPILE_FLAGS
                                       "-DUSE_ASM -DENABLE_SHANI \
                                       -DENABLE_SSE41 -DENABLE_AVX2")
