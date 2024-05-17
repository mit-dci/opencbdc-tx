set(secp256k1_CPPFLAGS -DUSE_NUM_NONE=1 -DUSE_FIELD_INV_BUILTIN=1
                       -DUSE_SCALAR_INV_BUILTIN=1 -DUSE_ECMULT_STATIC_PRECOMPUTATION=1
                       -DUSE_FIELD_5X52=1 -DUSE_SCALAR_4X64=1
                       -DECMULT_WINDOW_SIZE=15 -DECMULT_GEN_PREC_BITS=4
                       -DHAVE___INT128 -DENABLE_MODULE_RECOVERY=1
                       -DENABLE_MODULE_SCHNORRSIG=1 -DENABLE_MODULE_EXTRAKEYS=1
                       -DENABLE_MODULE_GENERATOR=1 -DENABLE_MODULE_BPPP=1)

add_library(secp256k1 secp256k1/src/precomputed_ecmult.c secp256k1/src/precomputed_ecmult_gen.c secp256k1/src/secp256k1.c)
target_include_directories(secp256k1 PRIVATE
                           secp256k1 secp256k1/src
                           ${CMAKE_CURRENT_BINARY_DIR}/src)
target_compile_options(secp256k1 PRIVATE ${secp256k1_CPPFLAGS})

add_subdirectory(crypto)
add_subdirectory(bech32)
