set(secp256k1_CPPFLAGS -DUSE_NUM_NONE=1 -DUSE_FIELD_INV_BUILTIN=1
                       -DUSE_SCALAR_INV_BUILTIN=1 -DUSE_ECMULT_STATIC_PRECOMPUTATION=1
                       -DUSE_FIELD_5X52=1 -DUSE_SCALAR_4X64=1
                       -DECMULT_WINDOW_SIZE=15 -DECMULT_GEN_PREC_BITS=4
                       -DHAVE___INT128 -DENABLE_MODULE_RECOVERY=1
                       -DENABLE_MODULE_SCHNORRSIG=1 -DENABLE_MODULE_EXTRAKEYS=1
                       -DENABLE_MODULE_GENERATOR=1 -DENABLE_MODULE_BULLETPROOFS=1)

set(secp256k1_genctx_SOURCES secp256k1/src/gen_context.c)

add_executable(secp256k1_genctx ${secp256k1_genctx_SOURCES})
target_include_directories(secp256k1_genctx PRIVATE secp256k1)
target_compile_options(secp256k1_genctx PRIVATE ${secp256k1_CPPFLAGS})

add_custom_command(OUTPUT src/ecmult_static_context.h
                   COMMAND mkdir -p src
                   COMMAND secp256k1_genctx
                   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
add_custom_target(secp256k1_genctx_run
                  DEPENDS src/ecmult_static_context.h)
add_dependencies(secp256k1_genctx_run secp256k1_genctx)

add_library(secp256k1 secp256k1/src/secp256k1.c)
target_include_directories(secp256k1 PRIVATE
                           secp256k1 secp256k1/src
                           ${CMAKE_CURRENT_BINARY_DIR}/src)
target_compile_options(secp256k1 PRIVATE ${secp256k1_CPPFLAGS})
add_dependencies(secp256k1 secp256k1_genctx_run)

add_subdirectory(crypto)
add_subdirectory(bech32)
