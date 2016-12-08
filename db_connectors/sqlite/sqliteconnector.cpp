/*
 * Copyright (c) 2016, SRCH2
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of the SRCH2 nor the
 *      names of its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SRCH2 BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * sqliteconnector.cpp
 *
 *  Created on: Jul 3, 2014
 */

#include "sqliteconnector.h"
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include "json/json.h"
#include "Logger.h"
#include <cstring>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include "io.h"

namespace {
#ifdef ANDROID
// For some reason, when we declare a string on 
// Android, it needs to be initialized to a non-empty string
const char* DEFAULT_STRING_VALUE = "DEFAULT";
#else
const char* DEFAULT_STRING_VALUE = "";
#endif
}

using srch2::util::Logger;

int indexedRecordsCount = 0;
int totalRecordsCount = 0;

SQLiteConnector::SQLiteConnector() {
    serverInterface = NULL;
    db = NULL;
    logRecordTimeChangedFlag = false;
    listenerWaitTime = 1;
    selectStmt = NULL;
    deleteLogStmt = NULL;
    lastAccessedLogRecordTimeStr = DEFAULT_STRING_VALUE;
}

//Initialize the connector. Establish a connection to Sqlite.
int SQLiteConnector::init(ServerInterface * serverInterface) {
    this->serverInterface = serverInterface;
    std::string tableName = DEFAULT_STRING_VALUE;
    this->serverInterface->configLookUp("tableName", tableName);
    //Set the default log table name and default log attributes name.
    LOG_TABLE_NAME = "SRCH2_LOG_";
    LOG_TABLE_NAME.append(tableName.c_str());
    LOG_TABLE_NAME_DATE = LOG_TABLE_NAME + "_DATE";
    LOG_TABLE_NAME_OP = LOG_TABLE_NAME + "_OP";
    //LOG_TABLE_NAME_ID column will store the old primary id
    //in case that the record primary id changed.
    LOG_TABLE_NAME_ID = LOG_TABLE_NAME + "_ID";
    TRIGGER_INSERT_NAME = "SRCH2_INSERT_LOG_TRIGGER_";
    TRIGGER_INSERT_NAME.append(tableName.c_str());
    TRIGGER_DELETE_NAME = "SRCH2_DELETE_LOG_TRIGGER_";
    TRIGGER_DELETE_NAME.append(tableName.c_str());
    TRIGGER_UPDATE_NAME = "SRCH2_UPDATE_LOG_TRIGGER_";
    TRIGGER_UPDATE_NAME.append(tableName.c_str());

    //Get listenerWaitTime value from the config file.
    std::string listenerWaitTimeStr = DEFAULT_STRING_VALUE;
    this->serverInterface->configLookUp("listenerWaitTime", listenerWaitTimeStr);
    listenerWaitTime = static_cast<int>(strtol(listenerWaitTimeStr.c_str(),
    NULL, 10));
    if (listenerWaitTimeStr.size() == 0 || listenerWaitTime == 0) {
        listenerWaitTime = 1;
    }

    /*
     * 1. Check if the config file has all the required parameters.
     * 2. Check if the SRCH2 engine can connect to Sqlite.
     * 3. Check if Sqlite contains the table that is specified in the config file
     *
     * If one of these checks failed, the init() fails and does not continue.
     */
    if (!checkConfigValidity() || !connectToDB() || !checkTableExistence()) {
        Logger::error("SQLITECONNECTOR: exiting...");
        return -1;
    }

    /*
     * 1. Get the table's schema and save them into a map<schema_name, schema_type>
     * 2. Create the log table to keep track of the changes in the original table.
     * 3. Create the triggers that can be invoked when the original table has changes.
     * 4. Create the prepared statements for the select/delete queries used in the listener.
     *
     * If one of these checks failed , the init() fails and does not continue.
     */
    if (!populateTableSchema() || !createLogTableIfNotExistence()
            || !createTriggerIfNotExistence() || !createPreparedStatement()) {
        Logger::error("SQLITECONNECTOR: exiting...");
        return -1;
    }

    return 0;
}

//Connect to the sqlite database
bool SQLiteConnector::connectToDB() {
    std::string db_name = DEFAULT_STRING_VALUE;
    std::string db_path = DEFAULT_STRING_VALUE;
    std::string srch2Home = DEFAULT_STRING_VALUE;
    this->serverInterface->configLookUp("db", db_name);
    this->serverInterface->configLookUp("dbPath", db_path);
    this->serverInterface->configLookUp("srch2Home", srch2Home);

    //Try to connect to the database.
    int rc;
    do {
        std::string path = srch2Home + "/" + db_path + "/" + db_name;
        rc = sqlite3_open(path.c_str(), &db);
        if (rc == 0) {
            return true;
        }

        Logger::error(
                "SQLITECONNECTOR: Can't open database file:%s, sqlite msg: %s",
                path.c_str(), sqlite3_errmsg(db));
        Logger::debug("SQLITECONNECTOR: trying again ...");
        sleep(listenerWaitTime);
    } while (1);

    return false;
}

/*
 * Check if the config file has all the required parameters.
 * e.g. if it contains dbname, table etc.
 * The config file must indicate the database path, db name, table name and
 * the primary key. Otherwise, the check fails.
 */
bool SQLiteConnector::checkConfigValidity() {
    std::string dbPath = DEFAULT_STRING_VALUE;
    std::string db = DEFAULT_STRING_VALUE;
    std::string uniqueKey = DEFAULT_STRING_VALUE;
    std::string tableName = DEFAULT_STRING_VALUE;
    this->serverInterface->configLookUp("dbPath", dbPath);
    this->serverInterface->configLookUp("db", db);
    this->serverInterface->configLookUp("uniqueKey", uniqueKey);
    this->serverInterface->configLookUp("tableName", tableName);

    bool ret = (dbPath.size() != 0) && (db.size() != 0)
            && (uniqueKey.size() != 0) && (tableName.size() != 0);
    if (!ret) {
        Logger::error("SQLITECONNECTOR: database path, db, tableName,"
                " uniquekey must be set.");
        return false;
    }

    return true;
}

//Check if database contains the table.
//Sqlite will return an error if the table doesn't exist.
//Query: SELECT COUNT(*) FROM table;
bool SQLiteConnector::checkTableExistence() {
    std::string tableName = DEFAULT_STRING_VALUE;
    this->serverInterface->configLookUp("tableName", tableName);

    /* Create SQL statement */
    char *zErrMsg = 0;
    std::stringstream sql;
    sql << "SELECT count(*) from " << tableName << ";";

    do {
        int rc = sqlite3_exec(db, sql.str().c_str(), NULL, NULL, &zErrMsg);
        if (rc == SQLITE_OK) {
            return true;
        }
        Logger::error("SQLITECONNECTOR: SQL error %d : %s", rc, zErrMsg);
        sqlite3_free(zErrMsg);

        Logger::debug("SQLITECONNECTOR: trying again ...");
        sleep(listenerWaitTime);
    } while (1);

    sqlite3_close(db);
    return false;
}

/*
 * Retrieve records from the table records and insert them into the SRCH2 engine.
 * Use the callback function "createIndex_callback" to process the records.
 * Each record will call "createIndex_callback" once.
 * Query: SELECT * FROM table;
 */
int SQLiteConnector::createNewIndexes() {
    std::string tableName = DEFAULT_STRING_VALUE;
    this->serverInterface->configLookUp("tableName", tableName);

    /* Create SQL statement */
    char *zErrMsg = 0;
    std::stringstream sql;
    sql << "SELECT * from " << tableName << ";";

    do {
        int rc = sqlite3_exec(db, sql.str().c_str(), addRecord_callback,
                (void *) this, &zErrMsg);\
        if (rc == SQLITE_OK) {
            Logger::info("SQLITECONNECTOR: Total indexed %d / %d records. ",
                    indexedRecordsCount, totalRecordsCount);

            std::stringstream ss;
            ss << time(NULL);
            lastAccessedLogRecordTimeStr = ss.str();

            return 0;
        }

        Logger::error("SQLITECONNECTOR: SQL error %d : %s", rc, zErrMsg);
        sqlite3_free(zErrMsg);

        Logger::debug("SQLITECONNECTOR: trying again ...");
        sleep(listenerWaitTime);
    } while (1);

    return -1;
}

/*
 * The callback function of createIndex. Each row of the table will call
 * this function once and do the insertion by the server handler.
 */
int addRecord_callback(void *dbConnector, int argc, char **argv,
        char **azColName) {
    totalRecordsCount++;
    SQLiteConnector * sqliteConnector = (SQLiteConnector *) dbConnector;

    Json::Value record;
    Json::FastWriter writer;

    for (int i = 0; i < argc; i++) {
        record[azColName[i]] = argv[i] ? argv[i] : "NULL";
    }
    std::string jsonString = writer.write(record);
    if (sqliteConnector->serverInterface->insertRecord(jsonString) == 0) {
        indexedRecordsCount++;
    }

    if (indexedRecordsCount && (indexedRecordsCount % 1000) == 0)
        Logger::info("SQLITECONNECTOR: Indexed %d records so far ...",
                indexedRecordsCount);
    return 0;
}

/*
 * Periodically check updates in the Sqlite log table, and send
 * corresponding requests to the SRCH2 engine
 */
int SQLiteConnector::runListener() {
    std::string tableName = DEFAULT_STRING_VALUE;
    this->serverInterface->configLookUp("tableName", tableName);

    //A timestamp that indicates the last time the SRCH2 engine
    //accessed the log table to retrieve the change history
    loadLastAccessedLogRecordTime();

    Json::Value record;
    Json::FastWriter writer;

    Logger::info("SQLITECONNECTOR: waiting for updates ...");
    bool fatal_error = false;
    do {
        /*
         * While loop of the listener. In each iteration it binds the prepared
         * statement value, fetch the record, save the indexes and update the
         * time stamp.
         */
        while (1) {
            logRecordTimeChangedFlag = false;

            int rc = sqlite3_bind_text(selectStmt, 1,
                    lastAccessedLogRecordTimeStr.c_str(),
                    lastAccessedLogRecordTimeStr.size(), SQLITE_STATIC);
            if (rc != SQLITE_OK) {
                Logger::error("SQLITECONNECTOR: SQL error %d : %s", rc,
                        sqlite3_errmsg(db));
                break;
            }

            /*
             * While loop of the query result. In each iteration it will
             *  handle only one record.
             */
            int ctotal = sqlite3_column_count(selectStmt);
            int res = 0;
            while (1) {
                res = sqlite3_step(selectStmt);
                if (res == SQLITE_ROW) {
                    /*
                     * Get old id, operation and time stamp of the log record.
                     * The order is same with the order when we create the log table
                     * 1 -> old id
                     * 2 -> time stamp
                     * 3 -> operation
                     */
                    std::string oldId = (char*) sqlite3_column_text(selectStmt,
                            0);
                    lastAccessedLogRecordTimeStr = (char*) sqlite3_column_text(
                            selectStmt, 1);
                    logRecordTimeChangedFlag = true;
                    char* op = (char*) sqlite3_column_text(selectStmt, 2);

                    //For loop of the attributes of one record.
                    //Create a Json format string for each record.
                    std::map<std::string, std::string>::iterator it =
                            tableSchema.begin();
                    int i = 3;
                    for (i = 3; i < ctotal && it != tableSchema.end();
                            i++, it++) {
                        char * val = (char*) sqlite3_column_text(selectStmt, i);
                        record[it->first.c_str()] = val ? val : "NULL";
                    }

                    /*
                     * The column count of the log table should be equal to
                     * the original table column count + 3 (LOG_TABLE_NAME_ID,
                     * LOG_TABLE_NAME_DATE,LOG_TABLE_NAME_OP)
                     * This error may happen if Log table/original
                     * table schema changed
                     */
                    if (it != tableSchema.end() || i != ctotal) {
                        Logger::error(
                                "SQLITECONNECTOR : Fatal Error. Table %s and"
                                        " log table %s are not consistent!",
                                tableName.c_str(), LOG_TABLE_NAME.c_str());
                        fatal_error = true;
                        break;
                    }

                    /*
                     * Generate the Json format string and call the
                     * corresponding operation based on the "op" field.
                     *
                     * 'i' -> Insertion
                     * 'd' -> Deletion
                     * 'u' -> Update
                     */
                    std::string jsonString = writer.write(record);

                    Logger::debug("SQLITECONNECTOR: Processing %s ",
                            jsonString.c_str());
                    if (strcmp(op, "i") == 0) {
                        serverInterface->insertRecord(jsonString);
                    } else if (strcmp(op, "d") == 0) {
                        serverInterface->deleteRecord(oldId);
                    } else if (strcmp(op, "u") == 0) {
                        serverInterface->updateRecord(oldId, jsonString);
                    }
                } else if (res == SQLITE_BUSY) {
                    //Retry if the database is busy.
                    //Wait for the next time to check the database
                    Logger::warn(
                            "SQLITECONNECTOR : SQLITE database is busy. Retrying");
                    sleep(listenerWaitTime);
                } else {
                    break;
                }
            }

            //If fatal error happens, exit the listener immediately.
            if (fatal_error) {
                break;
            }

            //Retry the connection if the sql error happens.
            if (sqlite3_errcode(db) != SQLITE_DONE
                    && sqlite3_errcode(db) != SQLITE_OK) {
                Logger::error("SQLITECONNECTOR: Error code SQL error %d : %s",
                        rc, sqlite3_errmsg(db));
                sqlite3_reset(selectStmt);
                break;
            }

            //Reset the prepared statement
            if (sqlite3_reset(selectStmt) != SQLITE_OK) {
                Logger::error("SQLITECONNECTOR: SQL error %d : %s", rc,
                        sqlite3_errmsg(db));
                break;
            }

            /*
             * Every record processed will change the flag to true. So the
             * connector could save the changes and update the log table.
             */
            if (logRecordTimeChangedFlag) {
                Logger::info("SQLITECONNECTOR: waiting for updates ...");
            }
            sleep(listenerWaitTime);
        }
        if (fatal_error) {
            break;
        }

        Logger::debug("SQLITECONNECTOR: trying again ...");
        sleep(listenerWaitTime);
    } while (1);

    Logger::info("SQLITECONNECTOR: exiting...");

    sqlite3_finalize(selectStmt);
    sqlite3_finalize(deleteLogStmt);
    sqlite3_close(db);
    return -1;
}

/*
 * Create the log table to keep track of the changes in the original table.
 * Query example: CREATE TABLE SRCH2_LOG_COMPANY(SRCH2_LOG_COMPANY_ID INT NOT NULL,
 *  SRCH2_LOG_COMPANY_DATE TIMESTAMP NOT NULL,
 *  SRCH2_LOG_COMPANY_OP CHAR(1) NOT NULL,
 *  ADDRESS CHAR(50) , AGE INT , ID INT , NAME TEXT , SALARY REAL );
 */
bool SQLiteConnector::createLogTableIfNotExistence() {
    /* Create SQL statement */
    char *zErrMsg = 0;
    std::stringstream sql;
    sql << "CREATE TABLE " << LOG_TABLE_NAME << "(" << LOG_TABLE_NAME_ID << " "
            << PRIMARY_KEY_TYPE << " NOT NULL, " << LOG_TABLE_NAME_DATE
            << " TIMESTAMP NOT NULL, " << LOG_TABLE_NAME_OP
            << " CHAR(1) NOT NULL,";

    for (std::map<std::string, std::string>::iterator it =
            this->tableSchema.begin(); it != this->tableSchema.end(); it++) {
        sql << " " << it->first << " " << it->second << " ";
        if (++it != this->tableSchema.end()) {
            sql << ",";
        }
        --it;
    }
    sql << ");";

    do {
        int rc = sqlite3_exec(db, sql.str().c_str(), NULL, 0, &zErrMsg);
        if ((rc != SQLITE_OK)
                && (std::string(zErrMsg).find("already exists")
                        == std::string::npos)) {
            Logger::error("SQLITECONNECTOR: SQL error %d : %s", rc, zErrMsg);
            sqlite3_free(zErrMsg);
        } else {
            return true;
        }

        Logger::debug("SQLITECONNECTOR: trying again ...");
        sleep(listenerWaitTime);
    } while (1);

    Logger::error("SQLITECONNECTOR: Create log table %s failed.",
            LOG_TABLE_NAME.c_str());
    return false;
}

//Return LOG_TABLE_NAME_DATE
const char* SQLiteConnector::getLogTableDateAttr() {
    return this->LOG_TABLE_NAME_DATE.c_str();
}

//Return LOG_TABLE_NAME_OP
const char* SQLiteConnector::getLogTableOpAttr() {
    return this->LOG_TABLE_NAME_OP.c_str();
}

//Return LOG_TABLE_NAME_ID
const char* SQLiteConnector::getLogTableIdAttr() {
    return this->LOG_TABLE_NAME_ID.c_str();
}

//Get the table's schema and save them into a map<schema_name, schema_type>
//Query: PRAGMA table_info(table_name);
bool SQLiteConnector::populateTableSchema() {
    std::string tableName = DEFAULT_STRING_VALUE;
    this->serverInterface->configLookUp("tableName", tableName);
    /* Create SQL statement */
    char *zErrMsg = 0;
    std::stringstream sql;
    sql << "PRAGMA table_info(" << tableName << ");";

    do {
        int rc = sqlite3_exec(db, sql.str().c_str(),
                populateTableSchema_callback, (void *) this, &zErrMsg);
        if (rc == SQLITE_OK) {
            return true;
        }

        Logger::error("SQLITECONNECTOR: SQL error %d : %s", rc, zErrMsg);
        sqlite3_free(zErrMsg);
        Logger::debug("SQLITECONNECTOR: trying again ...");
        sleep(listenerWaitTime);
    } while (1);

    Logger::error("SQLITECONNECTOR: Populate schema of table %s failed.",
            tableName.c_str());
    return false;
}

/*
 * The callback function of "populateTableSchema".
 * Only looking for the name, type, ifPrimaryKey of the attributes,
 * and save these information into a map<schema_name,schema_type>.
 */
int populateTableSchema_callback(void * dbConnector, int argc, char ** argv,
        char **azColName) {
    SQLiteConnector * sqliteConnector = (SQLiteConnector *) dbConnector;
    std::string key = DEFAULT_STRING_VALUE;
    std::string value = DEFAULT_STRING_VALUE;
    for (int i = 0; i < argc; i++) {
        if (strcmp(azColName[i], "name") == 0) {
            key = argv[i] ? argv[i] : "NULL";
        } else if (strcmp(azColName[i], "type") == 0) {
            value = argv[i] ? argv[i] : "NULL";
        } else if ((strcmp(azColName[i], "pk") == 0)
                && (strcmp(argv[i], "1") == 0)) {
            sqliteConnector->setPrimaryKeyType(value);
            sqliteConnector->setPrimaryKeyName(key);
        }
    }
    sqliteConnector->tableSchema[key] = value;
    return 0;
}

void SQLiteConnector::setPrimaryKeyType(const std::string& pkType) {
    this->PRIMARY_KEY_TYPE = pkType;
}

void SQLiteConnector::setPrimaryKeyName(const std::string& pkName) {
    this->PRIMARY_KEY_NAME = pkName;
}

/*
 * Create the prepared statements for the select/delete queries used in the listener.
 *
 * Select Query: SELECT * FROM log_table WHERE log_table_date > ?
 * ORDER BY log_table_date ASC;
 *
 * Delete Query: DELETE * FROM log_table WHERE log_table_date <= ?;
 */
bool SQLiteConnector::createPreparedStatement() {
    std::stringstream sql;

    //Create select prepared statement
    sql << "SELECT * from " << LOG_TABLE_NAME << " WHERE " << LOG_TABLE_NAME
            << "_DATE > ? ORDER BY " << LOG_TABLE_NAME << "_DATE ASC; ";

    do {
        int rc = sqlite3_prepare_v2(db, sql.str().c_str(), -1, &selectStmt, 0);
        if (rc == SQLITE_OK) {
            break;
        }
        Logger::error("SQLITECONNECTOR: SQL error %d : %s", rc,
                sqlite3_errmsg(db));
        Logger::debug("SQLITECONNECTOR: trying again ...");
        sleep(listenerWaitTime);
    } while (1);

    //Create delete prepared statement.
    sql.str("");
    sql << "DELETE FROM " << LOG_TABLE_NAME << " WHERE " << LOG_TABLE_NAME
            << "_DATE <= ? ;";

    do {
        int rc = sqlite3_prepare_v2(db, sql.str().c_str(), -1, &deleteLogStmt,
                0);
        if (rc == SQLITE_OK) {
            break;
        }
        Logger::error("SQLITECONNECTOR: SQL error %d : %s", rc,
                sqlite3_errmsg(db));
        Logger::debug("SQLITECONNECTOR: trying again ...");
        sleep(listenerWaitTime);
    } while (1);

    return true;
}

/*
 * Create the triggers that can be invoked when the original table has changes.
 *
 * Insert trigger query example: CREATE TRIGGER SRCH2_INSERT_LOG_TRIGGER_COMPANY
 * AFTER INSERT ON COMPANY BEGIN INSERT INTO SRCH2_LOG_COMPANY
 * VALUES (new.ID , strftime('%s', 'now'),'i', new.ADDRESS , new.AGE ,
 *  new.ID , new.NAME , new.SALARY );END;
 *
 * Delete trigger query example: CREATE TRIGGER SRCH2_DELETE_LOG_TRIGGER_COMPANY
 * AFTER DELETE ON COMPANY BEGIN INSERT INTO SRCH2_LOG_COMPANY
 * VALUES (old.ID , strftime('%s', 'now'),'d', old.ADDRESS , old.AGE ,
 *  old.ID , old.NAME , old.SALARY );END;
 *
 *  Update trigger query example: CREATE TRIGGER SRCH2_UPDATE_LOG_TRIGGER_COMPANY
 *  AFTER UPDATE ON COMPANY BEGIN INSERT INTO SRCH2_LOG_COMPANY
 *  VALUES (old.ID , strftime('%s', 'now'),'u', new.ADDRESS , new.AGE ,
 *  new.ID , new.NAME , new.SALARY );END;
 */
bool SQLiteConnector::createTriggerIfNotExistence() {
    std::string tableName = DEFAULT_STRING_VALUE;
    this->serverInterface->configLookUp("tableName", tableName);

    /* Insert Trigger Create SQL statement */
    char *zErrMsgInsert = 0;
    int rc = 0;
    std::stringstream sql;

    sql << "CREATE TRIGGER " << TRIGGER_INSERT_NAME << " AFTER INSERT ON "
            << tableName << " BEGIN "
                    "INSERT INTO " << LOG_TABLE_NAME << " VALUES (new."
            << PRIMARY_KEY_NAME << " , strftime('%s', 'now'),'i',";

    for (std::map<std::string, std::string>::iterator it =
            this->tableSchema.begin(); it != this->tableSchema.end(); it++) {
        sql << " new." << it->first << " ";
        if (++it != this->tableSchema.end()) {
            sql << ",";
        }
        --it;
    }
    sql << ");END;";

    do {
        rc = sqlite3_exec(db, sql.str().c_str(), NULL, 0, &zErrMsgInsert);
        if ((rc != SQLITE_OK)
                && (std::string(zErrMsgInsert).find("already exists")
                        == std::string::npos)) {
            Logger::error("SQLITECONNECTOR: SQL error %d : %s", rc,
                    zErrMsgInsert);
            sqlite3_free(zErrMsgInsert);
        } else {
            break;
        }

        Logger::debug("SQLITECONNECTOR: trying again ...");
        sleep(listenerWaitTime);
    } while (1);

    /* Delete Trigger Create SQL statement */
    sql.str("");
    char *zErrMsgDelete = 0;

    sql << "CREATE TRIGGER " << TRIGGER_DELETE_NAME << " AFTER DELETE ON "
            << tableName << " BEGIN "
                    "INSERT INTO " << LOG_TABLE_NAME << " VALUES (old."
            << PRIMARY_KEY_NAME << " , strftime('%s', 'now'),'d',";

    for (std::map<std::string, std::string>::iterator it =
            this->tableSchema.begin(); it != this->tableSchema.end(); it++) {
        sql << " old." << it->first << " ";
        if (++it != this->tableSchema.end()) {
            sql << ",";
        }
        --it;
    }
    sql << ");END;";

    do {
        rc = sqlite3_exec(db, sql.str().c_str(), NULL, 0, &zErrMsgDelete);
        if ((rc != SQLITE_OK)
                && (std::string(zErrMsgDelete).find("already exists")
                        == std::string::npos)) {
            Logger::error("SQLITECONNECTOR: SQL error %d : %s", rc,
                    zErrMsgDelete);
            sqlite3_free(zErrMsgDelete);
        } else {
            break;
        }

        Logger::debug("SQLITECONNECTOR: trying again ...");
        sleep(listenerWaitTime);
    } while (1);

    /* Update Trigger Create SQL statement */
    sql.str("");
    char *zErrMsgUpdate = 0;
    sql << "CREATE TRIGGER " << TRIGGER_UPDATE_NAME << " AFTER UPDATE ON "
            << tableName << " BEGIN "
                    "INSERT INTO " << LOG_TABLE_NAME << " VALUES (old."
            << PRIMARY_KEY_NAME << " , strftime('%s', 'now'),'u',";

    for (std::map<std::string, std::string>::iterator it =
            this->tableSchema.begin(); it != this->tableSchema.end(); it++) {
        sql << " new." << it->first << " ";
        if (++it != this->tableSchema.end()) {
            sql << ",";
        }
        --it;
    }
    sql << ");END;";

    do {
        rc = sqlite3_exec(db, sql.str().c_str(), NULL, 0, &zErrMsgUpdate);
        if ((rc != SQLITE_OK)
                && (std::string(zErrMsgUpdate).find("already exists")
                        == std::string::npos)) {
            Logger::error("SQLITECONNECTOR: SQL error %d : %s", rc,
                    zErrMsgUpdate);
            sqlite3_free(zErrMsgUpdate);
        } else {
            break;
        }

        Logger::debug("SQLITECONNECTOR: trying again ...");
        sleep(listenerWaitTime);
    } while (1);

    return true;
}

//Load the lastAccessedLogRecordTime from disk
void SQLiteConnector::loadLastAccessedLogRecordTime() {
    std::string path = DEFAULT_STRING_VALUE;
    std::string srch2Home = DEFAULT_STRING_VALUE;
    this->serverInterface->configLookUp("srch2Home", srch2Home);
    this->serverInterface->configLookUp("dataDir", path);
    path = srch2Home + "/" + path + "/" + "sqlite_data/data.bin";
    if (checkFileExisted(path.c_str())) {
        std::ifstream a_file(path.c_str(), std::ios::in | std::ios::binary);
        a_file >> lastAccessedLogRecordTimeStr;
        a_file.close();
    } else {
        if(lastAccessedLogRecordTimeStr.compare(DEFAULT_STRING_VALUE)==0){
            Logger::error("SQLITECONNECTOR: Can not find %s. The data may be"
                    "inconsistent. Please rebuild the indexes.",
                    path.c_str());
            std::stringstream ss;
            ss << time(NULL);
            lastAccessedLogRecordTimeStr = ss.str();
        }
    }
}

//Save lastAccessedLogRecordTime to disk
void SQLiteConnector::saveLastAccessedLogRecordTime() {
    deleteProcessedLog();

    std::string path = DEFAULT_STRING_VALUE;
    std::string srch2Home = DEFAULT_STRING_VALUE;
    this->serverInterface->configLookUp("srch2Home", srch2Home);
    this->serverInterface->configLookUp("dataDir", path);
    path = srch2Home + "/" + path + "/" + "sqlite_data/";
    if (!checkDirExisted(path.c_str())) {
        // S_IRWXU : Read, write, and execute by owner.
        // S_IRWXG : Read, write, and execute by group.
        mkdir(path.c_str(), S_IRWXU | S_IRWXG);
    }

    std::string pt = path + "data.bin";
    std::ofstream a_file(pt.c_str(), std::ios::trunc | std::ios::binary);
    a_file << lastAccessedLogRecordTimeStr;
    a_file.flush();
    a_file.close();
}

//Delete the processed log from the table so that we can keep it small.
bool SQLiteConnector::deleteProcessedLog() {

    //Bind the lastAccessedLogRecordTime
    int rc = sqlite3_bind_text(deleteLogStmt, 1,
            lastAccessedLogRecordTimeStr.c_str(), lastAccessedLogRecordTimeStr.size(),
            SQLITE_STATIC);
    if (rc != SQLITE_OK && rc != SQLITE_DONE) {
        Logger::error(
                "SQLITECONNECTOR: SQL deleteProcessedLog: bind lastAccessedLogRecordTimeStr error %d : %s",
                rc, sqlite3_errmsg(db));
        return false;
    }

    //Execute the delete query
    rc = sqlite3_step(deleteLogStmt);

    if (rc != SQLITE_OK && rc != SQLITE_DONE) {
        Logger::error(
                "SQLITECONNECTOR: SQL deleteProcessedLog: Execute delete query error %d : %s",
                rc, sqlite3_errmsg(db));
        sqlite3_reset(deleteLogStmt);
        return false;
    }

    //Reset the prepared statement
    rc = sqlite3_reset(deleteLogStmt);

    if (rc != SQLITE_OK && rc != SQLITE_DONE) {
        Logger::error(
                "SQLITECONNECTOR: SQL deleteProcessedLog: Reset the prepared statement error %d : %s",
                rc, sqlite3_errmsg(db));
        return false;
    }

    return true;
}

SQLiteConnector::~SQLiteConnector() {

}

/*
 * "create_t()" and "destroy_t(DataConnector*)" is called to create/delete
 * the instance of the connector. A simple example of implementing these
 * two function is here.
 *
 * extern "C" DataConnector* create() {
 *     return new YourDBConnector;
 * }
 *
 * extern "C" void destroy(DataConnector* p) {
 *     delete p;
 * }
 *
 * These two C APIs are used by the srch2-engine to create/delete the instance
 * in the shared library.
 * The engine will call "create()" to get the connector and call
 * "destroy" to delete it.
 */
extern "C" DataConnector* create() {
    return new SQLiteConnector;
}

extern "C" void destroy(DataConnector* p) {
    delete p;
}
