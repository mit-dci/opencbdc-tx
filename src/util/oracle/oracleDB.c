#include "oracleDB.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// string_buffer functions
// void string_buffer_init(string_buffer *sb);
// void string_buffer_append(string_buffer *sb, const char *str);
// void string_buffer_free(string_buffer *sb);

static int read_key_file(char *username, char *password, char *wallet_pw);
static int set_environment();

int OracleDB_init(OracleDB *db) {
    // set environment variables
    if(set_environment() != 0) {
        printf("[Oracle DB] Error setting environment.\n");
        return 1;
    }

    // Create environment
    db->status = OCIEnvCreate(&db->envhp, OCI_DEFAULT, NULL, NULL, NULL, NULL, 0, NULL);
    if (db->status != OCI_SUCCESS) {
        printf("[Oracle DB] OCIEnvCreate failed.\n");
        return 1;
    }

    // Read keys from key file
    if (read_key_file(db->username, db->password, db->wallet_pw) == 0) {
        printf("[Oracle DB] Read key file successfully.\n");
        printf("[Oracle DB] Username: %s\n", db->username);
    } else {
        printf("[Oracle DB] Error reading key file.\n");
        return 1;
    }

    return 0;
}

// connect to oracle database
int OracleDB_connect(OracleDB *db) {
    // Allocate handles
    OCIHandleAlloc(db->envhp, (void **)&db->errhp, OCI_HTYPE_ERROR, 0, NULL);
    OCIHandleAlloc(db->envhp, (void **)&db->srvhp, OCI_HTYPE_SERVER, 0, NULL);
    OCIHandleAlloc(db->envhp, (void **)&db->svchp, OCI_HTYPE_SVCCTX, 0, NULL);
    OCIHandleAlloc(db->envhp, (void **)&db->usrhp, OCI_HTYPE_SESSION, 0, NULL);

    // Attach to server
    db->status = OCIServerAttach(db->srvhp, db->errhp, (text *)"cbdcauto_low", strlen("cbdcauto_low"), OCI_DEFAULT);
    if(db->status != OCI_SUCCESS) {
        printf("[Oracle DB] Error attaching to server.\n");
        print_oci_error(db->errhp);
        return 1;
    }

    // Set attribute server context
    db->status = OCIAttrSet(db->svchp, OCI_HTYPE_SVCCTX, db->srvhp, 0, OCI_ATTR_SERVER, db->errhp);
    if(db->status != OCI_SUCCESS) {
        printf("[Oracle DB] Error setting server attribute.\n");
        print_oci_error(db->errhp);
        return 1;
    }

    // Set attribute session context
    db->status = OCIAttrSet(db->usrhp, OCI_HTYPE_SESSION, (void *)db->username, (ub4)strlen(db->username), OCI_ATTR_USERNAME, db->errhp);
    if(db->status != OCI_SUCCESS) {
        printf("[Oracle DB] Error setting username attribute.\n");
        print_oci_error(db->errhp);
        return 1;
    }
    db->status = OCIAttrSet(db->usrhp, OCI_HTYPE_SESSION, (void *)db->password, (ub4)strlen(db->password), OCI_ATTR_PASSWORD, db->errhp);
    if(db->status != OCI_SUCCESS) {
        printf("[Oracle DB] Error setting password attribute.\n");
        print_oci_error(db->errhp);
        return 1;
    }

    // Log in
    db->status = OCISessionBegin(db->svchp, db->errhp, db->usrhp, OCI_CRED_RDBMS, OCI_DEFAULT);
    if(db->status != OCI_SUCCESS) {
        printf("[Oracle DB] Error logging in.\n");
        print_oci_error(db->errhp);
        return 1;
    }
    db->status = OCIAttrSet(db->svchp, OCI_HTYPE_SVCCTX, db->usrhp, 0, OCI_ATTR_SESSION, db->errhp);
    if(db->status != OCI_SUCCESS) {
        printf("[Oracle DB] Error setting session attribute.\n");
        print_oci_error(db->errhp);
        return 1;
    }

    printf("[Oracle DB] Connected to Oracle Database.\n");
    return 0;
}

// Execute SQL
int OracleDB_execute(OracleDB *db, const char *sql_query) {
    OCIStmt *stmthp;

    // PREPARE STATEMENT SECTION
    // Allocate a statement handle
    db->status = OCIHandleAlloc(db->envhp, (void **)&stmthp, OCI_HTYPE_STMT, 0, NULL);
    if (db->status != OCI_SUCCESS) {
        printf("[Oracle DB] Error allocating statement handle\n");
        print_oci_error(db->errhp);
        return 1;
    }

    db->status = OCIStmtPrepare(stmthp, db->errhp, (text *)sql_query, (ub4)strlen(sql_query), OCI_NTV_SYNTAX, OCI_DEFAULT);
    if (db->status != OCI_SUCCESS) {
        printf("[Oracle DB] Error preparing SQL statement\n");
        print_oci_error(db->errhp);
        return 1;
    }

    // EXECUTE NON SELECT STATEMENT SECTION
    db->status = OCIStmtExecute(db->svchp, stmthp, db->errhp, 1, 0, NULL, NULL, OCI_DEFAULT);
    if (db->status != OCI_SUCCESS) {
        printf("[Oracle DB] Error executing SQL statement\n");
        print_oci_error(db->errhp);
        return 1;
    }
    printf("[Oracle DB] SQL statement executed successfully.\n");

    // Commit the transaction
    db->status = OCITransCommit(db->svchp, db->errhp, OCI_DEFAULT);
    if (db->status != OCI_SUCCESS) {
        printf("[Oracle DB] Error committing transaction\n");
        print_oci_error(db->errhp);
        return 1;
    }
    printf("[Oracle DB] Transaction committed successfully.\n");

    if (stmthp != NULL) {
        OCIHandleFree(stmthp, OCI_HTYPE_STMT);
    }
    return 0;
}

// Execute SQL with bind variables
int OracleDB_execute_bind(OracleDB *db, const char *sql_query, const char **bind_vars, int num_bind_vars) {
    OCIStmt *stmthp;

    // Allocate a statement handle
    db->status = OCIHandleAlloc(db->envhp, (void **)&stmthp, OCI_HTYPE_STMT, 0, NULL);
    if (db->status != OCI_SUCCESS) {
        printf("[Oracle DB] Error allocating statement handle\n");
        print_oci_error(db->errhp);
        return 1;
    }

    // Prepare the SQL statement with bind variables
    db->status = OCIStmtPrepare(stmthp, db->errhp, (text *)sql_query, (ub4)strlen(sql_query), OCI_NTV_SYNTAX, OCI_DEFAULT);
    if (db->status != OCI_SUCCESS) {
        printf("[Oracle DB] Error preparing SQL statement\n");
        print_oci_error(db->errhp);
        return 1;
    }

    // Bind the variables
    for (int i = 0; i < num_bind_vars; i++) {
        db->status = OCIBindByName(stmthp, (OCIBind **)&bind_vars[i], strlen(bind_vars[i]), NULL, 0, SQLT_STR, NULL, NULL, NULL, 0, NULL, OCI_DEFAULT);
        if (db->status != OCI_SUCCESS) {
            printf("[Oracle DB] Error binding variable %d\n", i + 1);
            print_oci_error(db->errhp);
            return 1;
        }
    }

    // Execute the SQL statement
    db->status = OCIStmtExecute(db->svchp, stmthp, db->errhp, 1, 0, NULL, NULL, OCI_DEFAULT);
    if (db->status != OCI_SUCCESS) {
        printf("[Oracle DB] Error executing bind SQL statement\n");
        print_oci_error(db->errhp);
        return 1;
    }

    printf("[Oracle DB] Bind SQL statement executed successfully.\n");

    // Commit the transaction
    db->status = OCITransCommit(db->svchp, db->errhp, OCI_DEFAULT);
    if (db->status != OCI_SUCCESS) {
        printf("[Oracle DB] Error committing transaction\n");
        print_oci_error(db->errhp);
        return 1;
    }

    printf("[Oracle DB] Transaction committed successfully.\n");

    // Free the statement handle
    if (stmthp != NULL) {
        OCIHandleFree(stmthp, OCI_HTYPE_STMT);
    }

    return 0;
}

    // How to use bind variables
    // Bind variables is faster for adding lists to data to a table.
    //
    // const char *sql_query = "INSERT INTO table (id, name) VALUES (:1, :2)";
    // const char *id_values[] = {"101", "102", "103", "104", "105"};
    // const char *name_values[] = {"John", "Alice", "Bob", "Eve", "Charlie"};

    // // Number of sets of bind variables
    // int num_sets = 5;
    // for (int i = 0; i < num_sets; i++) {
    //     const char *bind_vars[] = {id_values[i], name_values[i]};
    //     OracleDB_execute_bind(&db, sql_query, bind_vars, 2);
    // }


// Cleans up OCI handles
int OracleDB_clean_up(OracleDB *db) {
    if (db->usrhp) OCIHandleFree(db->usrhp, OCI_HTYPE_SESSION);
    if (db->svchp) OCIHandleFree(db->svchp, OCI_HTYPE_SVCCTX);
    if (db->srvhp) OCIHandleFree(db->srvhp, OCI_HTYPE_SERVER);
    if (db->errhp) OCIHandleFree(db->errhp, OCI_HTYPE_ERROR);
    if (db->envhp) OCIHandleFree(db->envhp, OCI_HTYPE_ENV);
    return 0;
}

// Disconnects from Oracle Database
int OracleDB_disconnect(OracleDB *db) {
    if (db->usrhp && db->svchp && db->errhp) OCISessionEnd(db->svchp, db->errhp, db->usrhp, OCI_DEFAULT);
    if (db->srvhp && db->errhp) OCIServerDetach(db->srvhp, db->errhp, OCI_DEFAULT);
    OracleDB_clean_up(db);
    printf("[Oracle DB] Disconnected from Oracle Database.\n");
    return 0;
}


// Prints OCI error
void print_oci_error(OCIError *errhp) {
    sb4 errcode = 0;
    text errbuf[512];
    OCIErrorGet((dvoid *)errhp, 1, (text *)NULL, &errcode, errbuf, sizeof(errbuf), OCI_HTYPE_ERROR);
    printf("[Oracle DB] Error %d: %s\n", errcode, errbuf);
}

// Reads Key File into username, password, and wallet_pw
int read_key_file(char *username, char *password, char *wallet_pw) {
    // print working directory
    FILE *key_file = fopen("key.txt", "r");
    if(!key_file) {
        // if file not found in the current directory, try the docker oracle directory
        key_file = fopen("/opt/tx-processor/build/src/util/oracle/key.txt", "r");
        if(!key_file) {
            printf("[Oracle DB] Error opening key file in both locations.\n");
            return 1;
        }
    }
    char line[256];
    while(fgets(line, sizeof(line), key_file)) {
        if(strncmp(line, "username", 8) == 0) {
            sscanf(line, "username%*[ ]=%*[ ]%s", username);
        } else if(strncmp(line, "password", 8) == 0) {
            sscanf(line, "password%*[ ]=%*[ ]%s", password);
        } else if(strncmp(line, "wallet_password", 15) == 0) {
            sscanf(line, "wallet_password%*[ ]=%*[ ]%s", wallet_pw);
        }
    }
    fclose(key_file);
    return 0;
}

// Sets environment variables
int set_environment() {
    // Set TNS_ADMIN environment variable
    printf("[Oracle DB] Setting TNS_ADMIN environment variable.\n");
    if(setenv("TNS_ADMIN", "/opt/tx-processor/build/src/util/oracle/wallet/", 1) != 0) {
        perror("Error setting TNS_ADMIN environment variable");
        return 1;
    }

    // Set LD_LIBRARY_PATH environment variable
    printf("[Oracle DB] Setting LD_LIBRARY_PATH environment variable.\n");
    if(setenv("LD_LIBRARY_PATH", "/opt/tx-processor/build/src/util/oracle/instantclient/", 1) != 0) {
        perror("Error setting LD_LIBRARY_PATH environment variable");
        return 1;
    }
    return 0;
}
