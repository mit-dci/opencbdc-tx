// testOracleDB.cpp
#include "oracleDB.h"
#include <iostream>

int main() {
    OracleDB db;
    if (OracleDB_init(&db) == 0) {
        std::cout << "OracleDB initialized successfully." << std::endl;
        // Call other functions as needed
        // ...
        OracleDB_disconnect(&db);
    } else {
        std::cout << "Failed to initialize OracleDB." << std::endl;
    }
    return 0;
}
