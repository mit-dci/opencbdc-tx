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

// void string_buffer_init(string_buffer *sb) {
//     sb->length = 0;
//     sb->capacity = 128;
//     sb->buffer = malloc(sb->capacity);
//     sb->buffer[0] = '\0';
// }

// void string_buffer_append(string_buffer *sb, const char *str) {
//     size_t len = strlen(str);
//     while (sb->length + len + 1 > sb->capacity) {
//         sb->capacity *= 2;
//         sb->buffer = realloc(sb->buffer, sb->capacity);
//     }
//     memcpy(sb->buffer + sb->length, str, len);
//     sb->length += len;
//     sb->buffer[sb->length] = '\0';
// }

// void string_buffer_free(string_buffer *sb) {
//     free(sb->buffer);
// }


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

int OracleDB_execute_sql_query(OracleDB *db, const char *sql_query) {
    OCIStmt *stmthp;
    // OCIDefine *defnp;

    // String buffer
    // string_buffer result;
    // string_buffer_init(&result);

    // // Check if the query is a SELECT statement
    // int is_select = (strncasecmp(sql_query, "SELECT", 6) == 0);

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

    // ub4 column_count = 0;
    // char** column_values = NULL;
    // ub2* column_lengths = NULL;

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

    // if(is_select) {
    //     // EXECUTE SELECT STATEMENT SECTION
    //     db->status = OCIStmtExecute(db->svchp, stmthp, db->errhp, 0, 0, NULL, NULL, OCI_DEFAULT);
    //     if (db->status != OCI_SUCCESS) {
    //         printf("Error executing SQL statement\n");
    //         print_oci_error(db->errhp);
    //         return NULL;
    //     }

    //     // COLUMN NAMES SECTION
    //     db->status = OCIAttrGet(stmthp, OCI_HTYPE_STMT, &column_count, 0, OCI_ATTR_PARAM_COUNT, db->errhp);
    //     if (db->status != OCI_SUCCESS) {
    //         printf("Error getting column count\n");
    //         print_oci_error(db->errhp);
    //         return NULL;
    //     }

    //     // Define output variables
    //     OCIDefine *defines[column_count];
    //     ub2 data_types[column_count];
    //     size_t data_sizes[column_count];
    //     // ub2 data_lengths[column_count];
    //     char column_names[column_count][30];
    //     column_values = malloc(column_count * sizeof(char*));
    //     column_lengths = malloc(column_count * sizeof(ub2));

    //     printf("Query executed successfully.\n");

    //     for (ub4 i = 0; i < column_count; ++i) {
    //         db->status = OCIParamGet(stmthp, OCI_HTYPE_STMT, db->errhp, (dvoid **)&defines[i], i + 1);
    //         if (db->status != OCI_SUCCESS) {
    //             printf("Error getting column parameter\n");
    //             print_oci_error(db->errhp);
    //             return NULL;
    //         }

    //         db->status = OCIAttrGet(defines[i], OCI_DTYPE_PARAM, &data_types[i], 0, OCI_ATTR_DATA_TYPE, db->errhp);
    //         if (db->status != OCI_SUCCESS) {
    //             printf("Error getting column data type\n");
    //             print_oci_error(db->errhp);
    //             return NULL;
    //         }

    //         db->status = OCIAttrGet(defines[i], OCI_DTYPE_PARAM, &data_sizes[i], 0, OCI_ATTR_DATA_SIZE, db->errhp);
    //         if (db->status != OCI_SUCCESS) {
    //             printf("Error getting column data size\n");
    //             print_oci_error(db->errhp);
    //             return NULL;
    //         }

    //         ub4 name_length;
    //         db->status = OCIAttrGet(defines[i], OCI_DTYPE_PARAM, &name_length, 0, OCI_ATTR_NAME, db->errhp);
    //         if (db->status != OCI_SUCCESS) {
    //             printf("Error getting column name length\n");
    //             print_oci_error(db->errhp);
    //             return NULL;
    //         }

    //         db->status = OCIAttrGet(defines[i], OCI_DTYPE_PARAM, column_names[i], &name_length, OCI_ATTR_NAME, db->errhp);
    //         if (db->status != OCI_SUCCESS) {
    //             printf("Error getting column name\n");
    //             print_oci_error(db->errhp);
    //             return NULL;
    //         }
    //     }

    //     // Print column names
    //     OCIParam *paramhp;
    //     ub4 num_cols;
    //     db->status = OCIAttrGet(stmthp, OCI_HTYPE_STMT, &num_cols, 0, OCI_ATTR_PARAM_COUNT, db->errhp);
    //     if (db->status != OCI_SUCCESS) {
    //         printf("Error getting column count\n");
    //         print_oci_error(db->errhp);
    //         return NULL;
    //     }

    //     // Print column names
    //     int column_width = 15;
    //     printf("%-*s", column_width, "Returned Data:\n");
    //     for (ub4 i = 1; i <= num_cols; ++i) {
    //         db->status = OCIParamGet(stmthp, OCI_HTYPE_STMT, db->errhp, (void **)&paramhp, i);
    //         if (db->status != OCI_SUCCESS) {
    //             printf("Error getting column parameter\n");
    //             print_oci_error(db->errhp);
    //             return NULL;
    //         }

    //         text *column_name;
    //         ub4 column_name_length;
    //         db->status = OCIAttrGet(paramhp, OCI_DTYPE_PARAM, &column_name, &column_name_length, OCI_ATTR_NAME, db->errhp);
    //         if (db->status != OCI_SUCCESS) {
    //             printf("Error getting column name\n");
    //             print_oci_error(db->errhp);
    //             return NULL;
    //         }

    //         printf("%-*s", column_width, column_name);

    //         // fill string buffer
    //         char column_buf[column_width + 1];
    //         snprintf(column_buf, sizeof(column_buf), "%-*s", column_width, column_name);
    //         string_buffer_append(&result, column_buf);
    //     }
    //     string_buffer_append(&result, "\n");

    //     // COLUMN VALUES SECTION
    //     printf("\n");
    //     // Define output variables and allocate memory for each column value
    //     for (ub4 col_idx = 0; col_idx < column_count; ++col_idx) {
    //         column_values[col_idx] = malloc((data_sizes[col_idx] + 1) * sizeof(char)); // +1 for null terminator
    //         memset(column_values[col_idx], 0, (data_sizes[col_idx] + 1) * sizeof(char)); // Ensure the string is null-terminated

    //         // check if the value is within the range of sb4
    //         if (data_sizes[col_idx] + 1 > INT_MAX) {
    //             printf("Data size exceeds the maximum allowed value.\n");
    //             goto cleanup;
    //         }

    //         db->status = OCIDefineByPos(stmthp, &defines[col_idx], db->errhp, col_idx + 1, column_values[col_idx], (sb4)(data_sizes[col_idx] + 1), SQLT_STR, &column_lengths[col_idx], 0, 0, OCI_DEFAULT);
    //         if (db->status != OCI_SUCCESS) {
    //             printf("Error defining column variable for column\n");
    //             print_oci_error(db->errhp);
    //             goto cleanup;
    //         }

    //         // Fetch and print rows
    //         while (1) {
    //             db->status = OCIStmtFetch2(stmthp, db->errhp, 1, OCI_FETCH_NEXT, 0, OCI_DEFAULT);
    //             if (db->status == OCI_NO_DATA) {
    //                 break;
    //             } else if (db->status != OCI_SUCCESS) {
    //                 printf("Error fetching data\n");
    //                 print_oci_error(db->errhp);
    //                 goto cleanup;
    //             }

    //             for (ub4 j = 0; j < column_count; ++j) {
    //                 printf("%-*s", column_width, column_values[j]);
    //                 // fill string buffer
    //                 char value_buf[column_width + 1];
    //                 snprintf(value_buf, sizeof(value_buf), "%-*s", column_width, column_values[j]);
    //                 string_buffer_append(&result, value_buf);
    //             }
    //             printf("\n");
    //             string_buffer_append(&result, "\n");
    //         }
    //     }

    // } else {
        // EXECUTE NON SELECT STATEMENT SECTION
        // db->status = OCIStmtExecute(db->svchp, stmthp, db->errhp, 1, 0, NULL, NULL, OCI_DEFAULT);
        // if (db->status != OCI_SUCCESS) {
        //     printf("Error executing SQL statement\n");
        //     print_oci_error(db->errhp);
        //     return NULL;
        // }
        // printf("SQL statement executed successfully\n");

        // // Commit the transaction
        // db->status = OCITransCommit(db->svchp, db->errhp, OCI_DEFAULT);
        // if (db->status != OCI_SUCCESS) {
        //     printf("Error committing transaction\n");
        //     print_oci_error(db->errhp);
        //     return NULL;
        // }
        // printf("Transaction committed successfully\n");
    // }

    // char *result_copy = (char *)malloc((result.length + 1) * sizeof(char));
    // memcpy(result_copy, result.buffer, result.length);
    // result_copy[result.length] = '\0';
    // free(result.buffer);
    // return result_copy;

// cleanup:
    // // Free dynamically allocated memory
    // for (ub4 i = 0; i < column_count; ++i) {
    //     free(column_values[i]);
    // }
    // free(column_values);
    // free(column_lengths);

    if (stmthp != NULL) {
        OCIHandleFree(stmthp, OCI_HTYPE_STMT);
    }
    return 0;
}

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
