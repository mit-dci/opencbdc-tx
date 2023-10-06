// oracleDB.h

#ifndef ORACLEDB_H
#define ORACLEDB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <oci.h>

// Define the OracleDB struct
typedef struct {
    OCIEnv *envhp;
    OCIServer *srvhp;
    OCISession *usrhp;
    OCISvcCtx *svchp;
    OCIError *errhp;

    sword status;

    char username[128];
    char password[128];
    char wallet_pw[128];
} OracleDB;

typedef struct {
    char *buffer;
    size_t length;
    size_t capacity;
} string_buffer;

// Declare the functions
int OracleDB_init(OracleDB *db);
void OracleDB_connect(OracleDB *db);
char* OracleDB_execute_sql_query(OracleDB *db, const char *sql_query);
int OracleDB_clean_up(OracleDB *db);
int OracleDB_disconnect(OracleDB *db);
void print_oci_error(OCIError *errhp);

#ifdef __cplusplus
}
#endif

#endif // ORACLEDB_H
