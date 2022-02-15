project(crypto)

add_library(crypto sha256_avx2.cpp
                   sha256_shani.cpp
                   sha256_sse4.cpp
                   sha256_sse41.cpp
                   sha256.cpp
                   siphash.cpp)
target_include_directories(crypto PRIVATE ".." ".")

if (USE_AVX2)
set_property(SOURCE sha256.cpp APPEND PROPERTY COMPILE_DEFINITIONS "ENABLE_AVX2")
set_source_files_properties(sha256_avx2.cpp PROPERTIES
                                            COMPILE_DEFINITIONS "ENABLE_AVX2"
                                            COMPILE_FLAGS "${AVX2_CXXFLAGS}")
endif()

if (USE_SHANI)
set_property(SOURCE sha256.cpp APPEND PROPERTY COMPILE_DEFINITIONS "ENABLE_SHANI")
set_source_files_properties(sha256_shani.cpp PROPERTIES
                                             COMPILE_DEFINITIONS "ENABLE_SHANI"
                                             COMPILE_FLAGS "${SHANI_CXXFLAGS}")
endif()

if (USE_SSE41)
set_property(SOURCE sha256.cpp APPEND PROPERTY COMPILE_DEFINITIONS "ENABLE_SSE41")
set_source_files_properties(sha256_sse41.cpp PROPERTIES
                                             COMPILE_DEFINITIONS "ENABLE_SSE41"
                                             COMPILE_FLAGS "${SSE41_CXXFLAGS}")
endif()

set_property(SOURCE sha256.cpp APPEND PROPERTY COMPILE_DEFINITIONS "USE_ASM")
