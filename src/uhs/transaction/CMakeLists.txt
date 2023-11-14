project(transaction)

include_directories(${CMAKE_SOURCE_DIR}/src/util/oracle) #ADDED
include_directories(${CMAKE_SOURCE_DIR}/src/util/oracle/instantclient/sdk/include)  #ADDED

add_library(transaction transaction.cpp
                        messages.cpp
                        validation.cpp
                        wallet.cpp)

target_link_libraries(transaction oracleDB) #ADDED
