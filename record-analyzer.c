/******************************************************************************
 * Includes
******************************************************************************/
#include "sqlite/sqlite3.h"

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
/*****************************************************************************/

/******************************************************************************
 * Definitions
******************************************************************************/
// Fetch the latest data from the server with
//  scp ted@192.168.1.63:/home/ted/repos/serial-logger/mx42_records.db ./mx42_records.db
#define DEFAULT_DATABASE_NAME               "mx42_records.db"
#define DEFAULT_SQLITE_OPEN_ARGS            SQLITE_OPEN_READONLY

#define EXIT_CODE_SUCCESS                   (0)
#define EXIT_CODE_ERR_PORT_OPEN             (-1)
#define EXIT_CODE_ERR_FILE_OPEN             (-2)
#define EXIT_CODE_ERR_DB_OPEN               (-3)
#define EXIT_CODE_ERR_DB_TABLE_CREATE       (-4)
#define EXIT_CODE_ERR_CL_OPTION             (-5)

#define COLUMN_MODEL                        (0)  // TEXT
#define COLUMN_ID                           (1)  // TEXT        (PK)
#define COLUMN_TIMESTAMP                    (2)  // NUMERIC     (PK)
#define COLUMN_COUNT                        (3)  // INTEGER
#define COLUMN_BATTERY                      (4)  // REAL
#define COLUMN_CH1_TYPE                     (5)  // INTEGER
#define COLUMN_CH1_VAL                      (6)  // REAL
#define COLUMN_CH2_TYPE                     (7)  // INTEGER
#define COLUMN_CH2_VAL                      (8)  // REAL
#define COLUMN_INT_TEMP                     (9)  // REAL
#define COLUMN_INT_RH                       (10) // REAL
#define COLUMN_STATUS_1                     (11) // INTEGER
#define COLUMN_STATUS_2                     (12) // INTEGER
#define COLUMN_CHECKSUM                     (13) // INTEGER

#define NUM_DEVICES                         (4)
#define DEVICE_ID_LEN                       (13)

#define SQL_STRING_MAX_LEN                  (256)

#define PACKET_INTERVAL_MAX_SECS            (65)

#define STATUS1_HISTORICAL_REC_FLAG         (64)

typedef union {
    float float_val;
    uint8_t bytes[4];
} float_bytes_t;
/*****************************************************************************/

/******************************************************************************
 * Globals
******************************************************************************/
sqlite3* db = NULL;
bool verbose = false;
// For testing.  This is the Unix timestamp for 12/01/2024 00:00:00
// time_t start_ts = 1733011200;
// All the MX42s were configured for one-minute intervals at around 10:55 on 
// 12/17/2024.  This will be used after development for missed packet analysis.
// Transmitter configs were modified on 12/25 (1735084800).
time_t start_ts = 1735084800; // 1734465600;
/*****************************************************************************/

/******************************************************************************
 * Private Function Declarations
******************************************************************************/
/*****************************************************************************/

/******************************************************************************
 * Handler Declarations
******************************************************************************/
void sigint_handler(int sig);
/*****************************************************************************/

/******************************************************************************
 * M A I N
******************************************************************************/
int main(int argc, char* argv[]) {
    char* db_name = NULL;
    int rc = 0;
    char * errMsg = NULL;
    sqlite3_stmt * stmt_ptr;
    char sql_string[SQL_STRING_MAX_LEN] = {0};
    char ids[NUM_DEVICES][DEVICE_ID_LEN];

    // Initialize the ids array
    for (uint8_t i = 0; i < NUM_DEVICES; i++) {
        memset(ids[i], '\0', DEVICE_ID_LEN);
    }
    

    // Parse the command-line flags
    int opt;
    while ((opt = getopt(argc, argv, "d:v")) != -1) {
        switch (opt) {
            case 'd':
                db_name = optarg;
                break;
            case 'v':
                verbose = true;
                break;
            case '?':
                printf("Unknown option\n");
                printf("Usage: %s [-d <database>] [-v]\n", argv[0]);
                exit(EXIT_CODE_ERR_CL_OPTION);
                break;
            default:
                abort();
        }
    }
    if (!db_name) {
        db_name = DEFAULT_DATABASE_NAME;
    }

    // Register signal handler to capture Ctrl-C
    signal(SIGINT, sigint_handler);

    if (verbose) {
        printf("Press Ctrl+C to exit.\n");
    }

    // Try opening the database file
    if (verbose) {
        printf("Opening database %s...\n", db_name);
    }
    rc = sqlite3_open_v2(db_name, &db, DEFAULT_SQLITE_OPEN_ARGS, NULL);

    // Check if the database was opened successfully
    if (SQLITE_OK != rc) {
        printf("Failed to open database %s.\n", db_name);
        sqlite3_close_v2(db);
        exit(EXIT_CODE_ERR_DB_OPEN);
    }

    if (verbose) {
        printf("Opened database.\n");
    }

    if (!verbose) {
        printf("db: %s\n", db_name);
    }
    
    // Get a list of unique IDs that have reported in
    rc = sqlite3_prepare_v2(db, "SELECT DISTINCT id FROM records ORDER BY id ASC;", 
        SQL_STRING_MAX_LEN, &stmt_ptr, NULL);

   if( rc!=SQLITE_OK ){
      fprintf(stderr, "SQL error: %s\n", errMsg);
      sqlite3_free(errMsg);
    }
    else {
        rc = sqlite3_step(stmt_ptr);
        int row = 0;
        while ((SQLITE_ROW == rc) && (row < NUM_DEVICES)) {
             char* id = (char*)sqlite3_column_text(stmt_ptr, 0);
            memcpy(ids[row], id, strlen(id));
                    
            rc = sqlite3_step(stmt_ptr);
            row++;
        }
    }

    for (uint8_t i = 0; i < NUM_DEVICES; i++) {
        printf("\n%s:\n", ids[i]);

        // Count the number of records since 12/1/24
        snprintf(sql_string, SQL_STRING_MAX_LEN, 
            "SELECT COUNT(id) FROM records WHERE timestamp > %ld AND id = '%s';", 
            start_ts, ids[i]);
        rc = sqlite3_prepare_v2(db, sql_string, SQL_STRING_MAX_LEN, &stmt_ptr, NULL);

        if( rc!=SQLITE_OK ){
            fprintf(stderr, "SQL error: %s\n", errMsg);
            sqlite3_free(errMsg);
        }
        else {
            rc = sqlite3_step(stmt_ptr);
            int num_recs = sqlite3_column_int(stmt_ptr, 0);
            printf("\t%d records\n", num_recs);
        }

        // Count the number of gaps greater than 1 minute and the number of
        // historical records
        uint32_t num_gaps = 0;
        uint32_t largest_gap = 0;
        uint32_t num_historical_recs = 0;

        // Prepare a statement to select only records for this ID with
        // timestamps greater than the start_time.
        snprintf(sql_string, SQL_STRING_MAX_LEN, 
            "SELECT * FROM records WHERE timestamp > %ld AND id = '%s' ORDER BY timestamp ASC;", 
            start_ts, ids[i]);
        rc = sqlite3_prepare_v2(db, sql_string, SQL_STRING_MAX_LEN, &stmt_ptr, NULL);

        if( rc!=SQLITE_OK ){
            fprintf(stderr, "SQL error: %s\n", errMsg);
            sqlite3_free(errMsg);
        }
        else {
            // Get the first record in the result set
            rc = sqlite3_step(stmt_ptr);

            // Grab the timestamp from this record
            time_t last_ts = sqlite3_column_int64(stmt_ptr, COLUMN_TIMESTAMP);
            while (SQLITE_ROW == rc) {
                // Get the next record in the result set
                rc = sqlite3_step(stmt_ptr);
                if (SQLITE_ROW == rc) {
                    // Grab this new timestamp and calculate the delta
                    time_t this_ts = sqlite3_column_int64(stmt_ptr, COLUMN_TIMESTAMP);
                    uint32_t time_delta = this_ts - last_ts;

                    // If the delta is greater than the packet interval, call
                    // this a gap.
                    if (time_delta > PACKET_INTERVAL_MAX_SECS) {
                        num_gaps++;
                    }

                    // Track the largest gap
                    if (time_delta > largest_gap) {
                        largest_gap = time_delta;
                    }

                    // Check if the status1 byte indicates this was a historical
                    // record
                    uint8_t status1 = (uint8_t)sqlite3_column_int(stmt_ptr, COLUMN_STATUS_1);
                    if (status1 & STATUS1_HISTORICAL_REC_FLAG) {
                        num_historical_recs++;
                    }
                    // Update the last timestamp before taking the next record
                    last_ts = this_ts;
                }
            }

            printf("\t%u gap%s\n", num_gaps, (1 == num_gaps ? "" : "s"));
            printf("\tlargest gap: %u mins\n", largest_gap / 60);
            printf("\t%u historical record%s\n", num_historical_recs, 
                (1 == num_historical_recs ? "" : "s"));
        }
    }



    sqlite3_close(db);
    
    return 0;
}
/*****************************************************************************/

/******************************************************************************
 * Private Function Definitions
******************************************************************************/
//-----------------------------------------------------------------------------
/*****************************************************************************/

/******************************************************************************
 * Callback and Handler Definitions
******************************************************************************/
void sigint_handler(int sig) {
    printf("\nClosing database...\n");
    if (db) {
        sqlite3_close_v2(db);
    }
    printf("Closed.\n");

    exit(0);
}
//-----------------------------------------------------------------------------
/*****************************************************************************/