#ifndef ORACLEDB_H
#define ORACLEDB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <oci.h>

// OracleDB struct
typedef struct {
    OCIEnv *envhp;      // environment handler
    OCIServer *srvhp;   // server handler
    OCISession *usrhp;  // user handler
    OCISvcCtx *svchp;   // service handler
    OCIError *errhp;    // error handler

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

// Functions
int OracleDB_init(OracleDB *db);
int OracleDB_connect(OracleDB *db);
int OracleDB_execute(OracleDB *db, const char *sql_query);
int OracleDB_execute_bind(OracleDB *db, const char *sql_query, const char **bind_vars, int num_bind_vars);
int OracleDB_clean_up(OracleDB *db);
int OracleDB_disconnect(OracleDB *db);
void print_oci_error(OCIError *errhp);
int read_key_file(char *username, char *password, char *wallet_pw);
int set_environment();

#ifdef __cplusplus
}
#endif

#endif  // ORACLEDB_H
