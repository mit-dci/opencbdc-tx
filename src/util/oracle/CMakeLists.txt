project(oracle)

set(CMAKE_C_STANDARD 99)
set(OCI_CLASS oracleDB.c)

# Absolute path to the instantclient directory
set(INSTANTCLIENT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/instantclient)

# Include directories for header files
include_directories(${INSTANTCLIENT_DIR}/sdk/include)

# Create library 'oracleDB'
add_library(oracleDB SHARED ${OCI_CLASS})

# Link lclntsh
target_link_libraries(oracleDB ${INSTANTCLIENT_DIR}/libclntsh.so)

# Runtime path
set_target_properties(oracleDB PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE INSTALL_RPATH "${INSTANTCLIENT_DIR}")
