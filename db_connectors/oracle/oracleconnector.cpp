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
#include <cstring>
#include "oracleconnector.h"
#include <stdlib.h>
#include <sstream>
#include <algorithm>    // std::max
#include "io.h"
#include <unistd.h>
#include "json/json.h"
#include <fstream>

#define ORACLE_DEFAULT_MAX_COLUMN_LEN (1024)

using namespace std;

OracleConnector::OracleConnector() {
    serverInterface = NULL;
    listenerWaitTime = 1;
    henv = 0;
    hdbc = 0;
    lastAccessedLogRecordRSID = -1;
    oracleMaxColumnLength = ORACLE_DEFAULT_MAX_COLUMN_LEN;
}

int OracleConnector::init(ServerInterface *serverInterface) {
    // 1. Pass the ServerInterface handle from the engine to the connector.
    this->serverInterface = serverInterface;

    // Get the listenerWaitTime value from the config file.
    std::string listenerWaitTimeStr;
    this->serverInterface->configLookUp("listenerWaitTime", listenerWaitTimeStr);
    listenerWaitTime = static_cast<int>(strtol(listenerWaitTimeStr.c_str(), NULL, 10));
    if (listenerWaitTimeStr.size() == 0 || listenerWaitTime == 0) {
        listenerWaitTime = 1;
    }

    // Get ORACLE_MAX_RECORD_LEN from the config file.
    std::string oracleMaxColumnLengthStr;
    this->serverInterface->configLookUp("oracleMaxColumnLength",
            oracleMaxColumnLengthStr);
    oracleMaxColumnLength = static_cast<int>(strtol(
            oracleMaxColumnLengthStr.c_str(),
            NULL, 10));
    if (oracleMaxColumnLengthStr.size() == 0 || oracleMaxColumnLength == 0) {
        oracleMaxColumnLength = ORACLE_DEFAULT_MAX_COLUMN_LEN;
    }

    // Add one for '\0'
    oracleMaxColumnLength++;

    /*
     * 2. Check if the config file has all the required parameters.
     * 3. Check if the SRCH2 engine can connect to Oracle.
     *
     * If one of these checks failed, the init() fails and does not continue.
     */
    if (!checkConfigValidity() || !connectToDB()) {
        printLog("exiting...", 1);
        return -1;
    }

    // 4. Get the schema information from the Oracle database.
    string tableName;
    this->serverInterface->configLookUp("tableName", tableName);
    if (!populateTableSchema(tableName)) {
        printLog("exiting...", 1);
        return -1;
    }

    return 0;
}

/*
 * Connect to the Oracle database by using unixODBC.
 * The data source is the name in /etc/odbcinst.ini
 */
bool OracleConnector::connectToDB() {
    string dataSource, user, password, server;
    this->serverInterface->configLookUp("server", server);
    this->serverInterface->configLookUp("dataSource", dataSource);
    this->serverInterface->configLookUp("user", user);
    this->serverInterface->configLookUp("password", password);

    std::stringstream sql;
    std::stringstream connectionString;
    SQLRETURN retcode;
    SQLHSTMT hstmt;

    while (1) {
        // Allocate environment handle
        retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);

        if ((retcode != SQL_SUCCESS) && (retcode != SQL_SUCCESS_WITH_INFO)) {
            printSQLError(hstmt);
            printLog("Allocate environment handle failed.", 1);
            sleep(listenerWaitTime);
            continue;
        }

        // Set the ODBC version environment attribute
        retcode = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION,
                (SQLPOINTER*) SQL_OV_ODBC3, 0);

        if ((retcode != SQL_SUCCESS) && (retcode != SQL_SUCCESS_WITH_INFO)) {
            printSQLError(hstmt);
            printLog("Set the ODBC version "
                    "environment attribute failed.", 1);
            sleep(listenerWaitTime);
            continue;
        }

        // Allocate connection handle
        retcode = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);

        if ((retcode != SQL_SUCCESS) && (retcode != SQL_SUCCESS_WITH_INFO)) {
            printSQLError(hstmt);
            printLog("Allocate connection handle failed.", 1);
            sleep(listenerWaitTime);
            continue;
        }

        // Set login timeout to 5 seconds
        SQLSetConnectAttr(hdbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER) 5, 0);

        // Connect to the database
        connectionString << "DRIVER={" << dataSource << "};SERVER=" << server
                << ";UID=" << user << ";PWD=" << password << ";";

        retcode = SQLDriverConnect(hdbc, NULL,
                (SQLCHAR*) connectionString.str().c_str(),
                connectionString.str().size(), NULL, 0, NULL,
                SQL_DRIVER_NOPROMPT);

        if ((retcode != SQL_SUCCESS) && (retcode != SQL_SUCCESS_WITH_INFO)) {
            printSQLError(hstmt);
            printLog("Connect to the Oracle database failed.", 1);
            sleep(listenerWaitTime);
            continue;
        }
        // Allocate statement handle
        if (!allocateSQLHandle(hstmt)) {
            sleep(listenerWaitTime);
            continue;
        }
        deallocateSQLHandle(hstmt);

        break;
    }
    return true;
}

// Log the SQL Server error msg if exists.
void OracleConnector::printSQLError(SQLHSTMT & hstmt) {
    unsigned char szSQLSTATE[10];
    SDWORD nErr;
    unsigned char msg[SQL_MAX_MESSAGE_LENGTH + 1];
    SWORD cbmsg;

    while (SQLError(henv, hdbc, hstmt, szSQLSTATE, &nErr, msg, sizeof(msg),
            &cbmsg) == SQL_SUCCESS) {
        std::stringstream logstream;
        logstream << "SQLSTATE=" << szSQLSTATE << ", Native error=" << nErr
                << ", msg='" << msg << "'";
        printLog(logstream.str().c_str(), 1);
    }
}

// Execute a query.
bool OracleConnector::executeQuery(SQLHSTMT & hstmt, const std::string & query) {
    SQLRETURN retcode = SQLExecDirect(hstmt, (SQLCHAR *) query.c_str(),
            static_cast<short int>(query.size()));

    if ((retcode != SQL_SUCCESS) && (retcode != SQL_SUCCESS_WITH_INFO)) {
        std::stringstream logstream;
        logstream << "Execute query '" << query.c_str() << "' failed";
        printLog(logstream.str().c_str(), 1);
        printSQLError(hstmt);
        return false;
    }
    return true;
}

/*
 * Check if the config file has all the required parameters.
 */
bool OracleConnector::checkConfigValidity() {
    string dataSource, user, uniqueKey, tableName, server, changeTableName,
            ownerName;
    this->serverInterface->configLookUp("server", server);
    this->serverInterface->configLookUp("dataSource", dataSource);
    this->serverInterface->configLookUp("user", user);
    this->serverInterface->configLookUp("uniqueKey", uniqueKey);
    this->serverInterface->configLookUp("tableName", tableName);
    this->serverInterface->configLookUp("ownerName", ownerName);
    this->serverInterface->configLookUp("changeTableName", changeTableName);

    bool ret = (server.size() != 0) && (dataSource.size() != 0)
            && (user.size() != 0) && (uniqueKey.size() != 0)
            && (tableName.size() != 0) && (changeTableName.size() != 0);
    if (!ret) {
        printLog(
                "Host Address(server), Data Source Config Name(dataSource),"
                        " user name(user), table name(tableName), change table name(changeTableName),"
                        " owner name(ownerName) and the primary key(uniqueKey) must be set.",
                1);
        return false;
    }

    return true;
}

// Get the table's schema and save them into a vector<schema_name>
// Query: SELECT COLUMN_NAME FROM ALL_TAB_COLUMNS
// WHERE TABLE_NAME = [TABLE] AND OWNER = [OWNERNAME];
//
// For example: table emp(id, name, age, salary).
// The schema vector will contain {id, name, age, salary}
bool OracleConnector::populateTableSchema(std::string & tableName) {
    SQLCHAR * sSchema = new SQLCHAR[oracleMaxColumnLength];
    SQLRETURN retcode;
    SQLHSTMT hstmt;

    std::string ownerName;
    this->serverInterface->configLookUp("ownerName", ownerName);

    std::stringstream sql;
    sql << "SELECT COLUMN_NAME FROM ALL_TAB_COLUMNS WHERE TABLE_NAME = UPPER('"
        << tableName << "') AND OWNER = UPPER('" + ownerName + "');";

    do {
        // Clear the vector, redo the query and bind the columns
        vector<string>().swap(fieldNames);
        // Allocate statement handle
        if (!allocateSQLHandle(hstmt)) {
            sleep(listenerWaitTime);
            continue;
        }

        if (!executeQuery(hstmt, sql.str())) {
            sleep(listenerWaitTime);
            continue;
        }

        // Bind columns
        retcode = SQLBindCol(hstmt, 1, SQL_C_CHAR, sSchema,
                oracleMaxColumnLength,
                NULL);

        // Fetch and save each row in the schema. In case of an error,
        // display a message and exit. 
        bool sqlErrorFlag = false;
        int i = 0;
        while (1) {
            retcode = SQLFetch(hstmt);
            if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {
                printSQLError(hstmt);
                sleep(listenerWaitTime);
                sqlErrorFlag = true;
                break;
            } else if (retcode == SQL_SUCCESS
                    || retcode == SQL_SUCCESS_WITH_INFO) {
                fieldNames.push_back(string((char *) sSchema));
            } else
                break;
        }

        if (!sqlErrorFlag) {
            deallocateSQLHandle(hstmt);
            break;
        }

    } while (connectToDB());

    delete sSchema;
    return true;
}

/*
 * Retrieve records from the table and insert them into the SRCH2 engine.
 */
int OracleConnector::createNewIndexes() {
    std::string tableName, ownerName;
    this->serverInterface->configLookUp("tableName", tableName);
    this->serverInterface->configLookUp("ownerName", ownerName);

    int indexedRecordsCount = 0;
    int totalRecordsCount = 0;

    // Initialize the record buffer.
    vector<SQLCHAR *> sqlRecord;
    for (int i = 0; i < fieldNames.size(); i++) {
        SQLCHAR * sqlCol = new SQLCHAR[oracleMaxColumnLength];
        sqlRecord.push_back(sqlCol);
    }

    Json::Value record;
    Json::FastWriter writer;

    SQLRETURN retcode;
    SQLHSTMT hstmt;
    do {
        totalRecordsCount = 0;
        int colPosition = 1;

        // Allocate a statement handle
        if (!allocateSQLHandle(hstmt)) {
            // Use a parameter from the SRCH2 configuration file
            // to decide how long it should sleep
            sleep(listenerWaitTime);
            continue;
        }
        if (!executeQuery(hstmt,
                string("SELECT * FROM " + ownerName + "." + tableName))) {
            sleep(listenerWaitTime);
            continue;
        }

        // Bind columns
        for (vector<SQLCHAR *>::iterator it = sqlRecord.begin();
                it != sqlRecord.end(); ++it) {
            retcode = SQLBindCol(hstmt, colPosition++, SQL_C_CHAR, *it,
                    oracleMaxColumnLength,
                    NULL);
        }

        // Each loop will fetch one record.
        bool sqlErrorFlag = false;
        while (1) {
            retcode = SQLFetch(hstmt);  // Fetch one record
            if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {
                printSQLError(hstmt);
                sleep(listenerWaitTime);
                sqlErrorFlag = true;
                break;
            } else if (retcode == SQL_SUCCESS
                    || retcode == SQL_SUCCESS_WITH_INFO) {
                // Generate a JSON string from the record.
                vector<string>::iterator itName = fieldNames.begin();
                vector<SQLCHAR *>::iterator itValue = sqlRecord.begin();

                while (itName != fieldNames.end()) {
                    // trim the string
                    string val(reinterpret_cast<const char*>(*itValue));
                    val.erase(val.find_last_not_of(" \n\r\t") + 1);
                    record[*itName] = val;

                    itName ++;
                    itValue ++;
                }

                string jsonString = writer.write(record);
                totalRecordsCount++;
                if (serverInterface->insertRecord(jsonString) == 0) {
                    indexedRecordsCount++;
                }
                printLog(jsonString, 4);

                if (indexedRecordsCount && (indexedRecordsCount % 1000) == 0) {
                    std::stringstream logstream;
                    logstream << "Indexed " << indexedRecordsCount
                            << " records so far ...";
                    printLog(logstream.str().c_str(), 3);
                }
            } else
                break;
        }

        /*
         * If there is an error while creating the new indexes, the connector will
         * re-try connecting to the database.
         */
        if (!sqlErrorFlag) {
            populateLastAccessedLogRecordTime();
            deallocateSQLHandle(hstmt);
            break;
        }

    } while (connectToDB());

    std::stringstream logstream;
    logstream << "Total indexed  " << indexedRecordsCount << " / "
            << totalRecordsCount << " records. ";
    printLog(logstream.str().c_str(), 3);

    // Deallocate the record buffer.
    for (int i = 0; i < fieldNames.size(); i++) {
        delete sqlRecord[i];
    }

    return 0;
}

/*
 * Periodically pull the updates from the Oracle change table, and send
 * corresponding requests to the SRCH2 engine.
 */
int OracleConnector::runListener() {
    std::string changeTableName, uniqueKey, ownerName;
    this->serverInterface->configLookUp("changeTableName", changeTableName);
    this->serverInterface->configLookUp("uniqueKey", uniqueKey);
    this->serverInterface->configLookUp("ownerName", ownerName);

    /*
     * Load the last accessed record RSID$ from the file.
     */
    loadLastAccessedLogRecordTime();

    Json::Value record;
    Json::FastWriter writer;

    std::stringstream sql;
    SQLRETURN retcode;
    SQLHSTMT hstmt;

    // Initialize the record buffer.
    vector<SQLCHAR *> sqlRecord;
    vector<SQLLEN> sqlCallBack;
    for (int i = 0; i < fieldNames.size() + 3; i++) {
        SQLCHAR * sqlCol = new SQLCHAR[oracleMaxColumnLength];
        sqlRecord.push_back(sqlCol);
        sqlCallBack.push_back(0);
    }

    do {
        // Allocate statement handle
        if (!allocateSQLHandle(hstmt)) {
            // "listenerWaitTime" is from the SRCH2 config file
            sleep(listenerWaitTime);
            continue;
        }

        /*
         * Create the prepared select statement. Because the SRCH2 engine only
         * supports atomic primary keys but not compound primary keys, we
         * assume the primary key of the table only has one attribute.
         *
         * For example: table emp(id, name, age, salary).
         * "Change table" name : emp_ct
         * Query : SELECT RSID$, OPERATION$, id, name, age, salary
         *         FROM emp_ct
         *         WHERE RSID$ > ?;
         *
         * RSID$ is the "timestamp", which increases automatically
         * for each transaction that happens in the table emp.
         *
         * OPERATION$ has 4 options, "I ", "D ", "UO"(or "UU"), and "UN", which represent
         * INSERT, DELETE, UPDATE Old Value, and UPDATE New Value, respectively.
         *
         */
        sql.str("");
        sql << "SELECT RSID$, OPERATION$";

        for (vector<string>::iterator it = fieldNames.begin();
                it != fieldNames.end(); ++it) {
            sql << ", " << *it;
        }

        sql << " FROM " << ownerName << "." << changeTableName
                << " WHERE RSID$ > ? ORDER BY RSID$ ASC;";

        retcode = SQLPrepare(hstmt, (SQLCHAR*) sql.str().c_str(),
        SQL_NTS);

        // Bind the result set columns
        int colPosition = 1;
        vector<SQLLEN>::iterator itCallBack = sqlCallBack.begin();
        for (vector<SQLCHAR *>::iterator it = sqlRecord.begin();
                it != sqlRecord.end(); ++it) {
            retcode = SQLBindCol(hstmt, colPosition++,
            SQL_C_CHAR, *it, oracleMaxColumnLength, &(*itCallBack++));
        }

        /*
         * Loop for the run listener, and the loop will be executed every
         * "listenerWaitTime" seconds.
         */
        printLog("waiting for updates ...", 3);
        while (1) {
            // Bind the "lastAccessedLogRecordRSID"
            std::string lastAccessedLogRecordRSIDStr;
            std::stringstream strstream;
            strstream << lastAccessedLogRecordRSID;
            strstream >> lastAccessedLogRecordRSIDStr;

            retcode = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
            SQL_C_CHAR,
            SQL_CHAR, oracleMaxColumnLength, 0,
                    (SQLPOINTER) lastAccessedLogRecordRSIDStr.c_str(),
                    lastAccessedLogRecordRSIDStr.size(), NULL);

            // Execute the prepared statement.
            retcode = SQLExecute(hstmt);
            if ((retcode != SQL_SUCCESS)
                    && (retcode != SQL_SUCCESS_WITH_INFO)) {
                printLog("Executing prepared "
                        "statement failed in runListener().", 1);
                printSQLError(hstmt);
                // Re-connect to the SQL Server, and start again.
                sleep(listenerWaitTime);
                break;
            }

            bool sqlErrorFlag = false;
            bool UOFlag = false;
            bool UNFlag = false;
            string oldPk;   // Primary key of UO (For update)
            string newJSON; // New content of UN (For update)

            long int maxRSID = -1;
            // Each loop will fetch one record.
            while (1) {
                retcode = SQLFetch(hstmt);
                if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {
                    sqlErrorFlag = true;
                    printLog("Fetching records failed"
                            " in runListener().", 1);
                    printSQLError(hstmt);
                    sleep(listenerWaitTime);
                    break;
                } else if (retcode == SQL_SUCCESS
                        || retcode == SQL_SUCCESS_WITH_INFO) {
                    // Generate a JSON string from the record.
                    vector<string>::iterator itName = fieldNames.begin();
                    vector<SQLCHAR *>::iterator itValue = sqlRecord.begin();

                    // Get the current RSID
                    long int RSID = strtol(
                            reinterpret_cast<const char*>(*itValue++), NULL,
                            10);

                    // Keep the max RSID
                    maxRSID = max(maxRSID, RSID);

                    // Get the type of operation
                    string operation(reinterpret_cast<const char*>(*itValue));
                    itValue++;

                    string pkValue; //Primary key
                    while (itName != fieldNames.end()) {
                        // trim the string
                        string val(reinterpret_cast<const char*>(*itValue));
                        val.erase(val.find_last_not_of(" \n\r\t") + 1);

                        record[*itName] = val;
                        if ((*itName).compare(uniqueKey) == 0) {
                            pkValue = val;
                        }
                        itName++;
                        itValue++;
                    }
                    string jsonString = writer.write(record);
                    std::stringstream logstream;
                    logstream << "Change version : " << RSID << " ,operation : "
                            << operation.c_str() << ", Record: " << jsonString;
                    printLog(logstream.str().c_str(), 4);

                    // Make the changes to the SRCH2 indexes.
                    if (operation.compare("I ") == 0) {
                        // These functions are provided by the interface
                        // of the SRCH2 engine
                        serverInterface->insertRecord(jsonString);
                    } else if (operation.compare("D ") == 0) {
                        serverInterface->deleteRecord(pkValue);
                    } else if (operation.compare("UU") == 0
                            || operation.compare("UO") == 0) {
                        UOFlag = true;
                        oldPk = pkValue;    //UU/UO is old value,
                                            //we only care about the primary key of old record.
                    } else if (operation.compare("UN") == 0) {
                        UNFlag = true;
                        newJSON = jsonString;
                    } else {
                        logstream.str("");
                        logstream << "Error while parsing the SQL record "
                                << jsonString << " with operation "
                                << operation;
                        printLog(logstream.str().c_str(), 1);
                    }

                    // Update the indexes if both UN and UO are found.
                    if(UOFlag == true && UNFlag == true){
                        UOFlag = false; UNFlag = false;
                        serverInterface->updateRecord(oldPk, newJSON);
                    }
                } else {
                    break;
                }
            }

            /*
             * An error happened while fetching the columns from
             * Oracle, so try to re-connect to the database.
             */
            if (sqlErrorFlag) {
                break;
            } else {
                if (maxRSID != -1) {
                    printLog("waiting for updates ...", 3);
                    lastAccessedLogRecordRSID = maxRSID;
                }
            }
            SQLCloseCursor(hstmt);
            sleep(listenerWaitTime);
        }

        deallocateSQLHandle(hstmt);
    } while (connectToDB());

    // Deallocate the record buffer.
    for (int i = 0; i < sqlRecord.size(); i++) {
        delete sqlRecord[i];
    }

    return -1;
}

/*
 * Load the last accessed record RSID$ from the file.
 */
void OracleConnector::loadLastAccessedLogRecordTime() {
    std::string path, srch2Home;
    this->serverInterface->configLookUp("srch2Home", srch2Home);
    this->serverInterface->configLookUp("dataDir", path);

    path = srch2Home + "/" + path + "/" + "oracle_data/data.bin";
    if (checkFileExisted(path.c_str())) {
        std::ifstream a_file(path.c_str(), std::ios::in | std::ios::binary);
        a_file >> lastAccessedLogRecordRSID;
        a_file.close();
    } else {
        // The file does not exist but the indexes already exist.
        if (lastAccessedLogRecordRSID == -1) {
            std::stringstream logstream;
            logstream << "Can not find " << path
                    << ". The data may be inconsistent. Please rebuild the indexes.";
            printLog(logstream.str().c_str(), 1);
            populateLastAccessedLogRecordTime();
        }
    }
}

// Get the MAX RSID$ from Oracle database.
// Query : "SELECT MAX(RSID$) FROM changeTable";
void OracleConnector::populateLastAccessedLogRecordTime() {
    string changeTableName, ownerName;
    this->serverInterface->configLookUp("changeTableName", changeTableName);
    this->serverInterface->configLookUp("ownerName", ownerName);

    std::stringstream sql;
    sql << "SELECT MAX(RSID$) FROM " << ownerName << "." << changeTableName
            << ";";
    SQLHSTMT hstmt;
    SQLCHAR * changeVersion = new SQLCHAR[oracleMaxColumnLength];
    do {
        // Allocate a statement handle
        if (!allocateSQLHandle(hstmt)) {
            sleep(listenerWaitTime);
            continue;
        }
        if (!executeQuery(hstmt, sql.str())) {
            sleep(listenerWaitTime);
            continue;
        }

        SQLLEN callBack = 0;
        SQLRETURN retcode = SQLBindCol(hstmt, 1, SQL_C_CHAR, changeVersion,
                oracleMaxColumnLength, &callBack);
        retcode = SQLFetch(hstmt);
        if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
            lastAccessedLogRecordRSID = strtol((char*) changeVersion,
            NULL, 10);
            deallocateSQLHandle(hstmt);
            break;
        } else if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {
            printSQLError(hstmt);
            sleep(listenerWaitTime);
        } else {
            lastAccessedLogRecordRSID = 0;
            break;
        }
    } while (connectToDB());

    delete changeVersion;
}

// Save the lastAccessedLogRecordRSID to the file.
// For Oracle, we save the RSID instead of a timestamp.
void OracleConnector::saveLastAccessedLogRecordTime() {
    std::string path, srch2Home;
    this->serverInterface->configLookUp("srch2Home", srch2Home);
    this->serverInterface->configLookUp("dataDir", path);
    path = srch2Home + "/" + path + "/" + "oracle_data/";
    if (!checkDirExisted(path.c_str())) {
        // S_IRWXU : Read, write, and execute by owner.
        // S_IRWXG : Read, write, and execute by group.
        mkdir(path.c_str(), S_IRWXU | S_IRWXG);
    }
    std::string pt = path + "data.bin";
    std::ofstream a_file(pt.c_str(), std::ios::trunc | std::ios::binary);
    a_file << lastAccessedLogRecordRSID;
    a_file.flush();
    a_file.close();
}

// Allocate a SQL Server statement handle to execute the query
bool OracleConnector::allocateSQLHandle(SQLHSTMT & hstmt) {
    SQLRETURN retcode;

    // Allocate a statement handle
    retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);

    if ((retcode != SQL_SUCCESS) && (retcode != SQL_SUCCESS_WITH_INFO)) {
        printSQLError(hstmt);
        printLog("Allocate connection handle failed.", 1);
        return false;
    }
    return true;
}

// Deallocate an Oracle statement handle to free the memory
void OracleConnector::deallocateSQLHandle(SQLHSTMT & hstmt) {
    SQLCancel(hstmt);
    SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
}


// Print Log information.
void OracleConnector::printLog(const std::string & log, const int logLevel) {
    std::stringstream logstream;

    switch (logLevel) {
    case 1:
        logstream << "ERROR ORACLECONNECTOR: " << log;
        break;
    case 2:
        logstream << "WARN  ORACLECONNECTOR: " << log;
        break;
    case 3:
        logstream << "INFO  ORACLECONNECTOR: " << log;
        break;
    case 4:
        logstream << "DEBUG ORACLECONNECTOR: " << log;
        break;
    default:
        logstream << "DEBUG ORACLECONNECTOR: " << log;
    }
    logstream << "\n";
    printf(logstream.str().c_str());
}

// Disconnect from the Oracle
OracleConnector::~OracleConnector() {
    SQLDisconnect(hdbc);

    SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
    SQLFreeHandle(SQL_HANDLE_ENV, henv);
}

/*
 * "create_t()" and "destroy_t(DataConnector*)" are called to create/delete
 * the instance of the connector, respectively. A simple example of
 * implementing these two functions is here.
 *
 * extern "C" DataConnector* create() {
 *     return new YourDBConnector;
 * }
 *
 * extern "C" void destroy(DataConnector* p) {
 *     delete p;
 * }
 *
 * These two C APIs are used by the SRCH2 engine to create/delete the
 * instance in the shared library.  The engine will call "create()" to
 * get the connector and call "destroy" to delete it.
 */
extern "C" DataConnector* create() {
    return new OracleConnector;
}

extern "C" void destroy(DataConnector* p) {
    delete p;
}

