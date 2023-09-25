project(oracle)

set(CMAKE_C_STANDARD 99)
set(OCI_CLASS oracleDB.c)

# Include directories for header files
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/instantclient/sdk/include)

# Create library 'oracleDB'
add_library(oracleDB SHARED ${OCI_CLASS})

# Link lclntsh
target_link_libraries(oracleDB ${CMAKE_SOURCE_DIR}/instantclient/libclntsh.so)

# Runtime path
set_target_properties(oracleDB PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE INSTALL_RPATH "${CMAKE_SOURCE_DIR}/instantclient")
