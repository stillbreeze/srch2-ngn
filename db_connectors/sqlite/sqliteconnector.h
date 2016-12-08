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
 * sqliteconnector.h
 *
 *  Created on: Jul 3, 2014
 */

#ifndef __SQLITECONNECTOR_H__
#define __SQLITECONNECTOR_H__

#include "DataConnector.h"
#include <string>
#include <map>
#include <sqlite3.h>

//The callback function of createIndex.
int addRecord_callback(void * dbConnector, int argc, char ** argv,
        char **azColName);
//The callback function of populateTableSchema.
int populateTableSchema_callback(void * dbConnector, int argc, char ** argv,
        char **azColName);

class SQLiteConnector: public DataConnector {
public:
    SQLiteConnector();
    virtual ~SQLiteConnector();

    //Initialize the connector. Establish a connection to the Sqlite.
    virtual int init(ServerInterface *serverInterface);
    //Retrieve records from the table records and insert them into the SRCH2 engine.
    virtual int createNewIndexes();
    //Periodically check updates in the Sqlite log table,
    //and send corresponding requests to the SRCH2 engine.
    virtual int runListener();

    //Save the lastAccessedLogRecordTime to the disk
    virtual void saveLastAccessedLogRecordTime();

    //Return LOG_TABLE_NAME_DATE
    const char * getLogTableDateAttr();
    //Return LOG_TABLE_NAME_OP
    const char * getLogTableOpAttr();
    //Return LOG_TABLE_NAME_ID
    const char * getLogTableIdAttr();

    void setPrimaryKeyType(const std::string& pkType);
    void setPrimaryKeyName(const std::string& pkName);

    //Store the table schema. Key is schema name and value is schema type
    std::map<std::string, std::string> tableSchema;
    ServerInterface *serverInterface;
private:
    //Config parameters
    std::string LOG_TABLE_NAME;
    std::string TRIGGER_INSERT_NAME;
    std::string TRIGGER_DELETE_NAME;
    std::string TRIGGER_UPDATE_NAME;
    std::string LOG_TABLE_NAME_DATE;
    std::string LOG_TABLE_NAME_OP;
    std::string LOG_TABLE_NAME_ID;
    std::string PRIMARY_KEY_TYPE;
    std::string PRIMARY_KEY_NAME;
    int listenerWaitTime;

    /* For the sqlite, the connector is using this timestamp on the
     * prepared statement, so we do not need to convert it to type time_t
     * in the runListener() for each loop. To be consistent with other connectors,
     * we use "lastAccessedLogRecordTimeStr" instead of "lastAccessedLogRecordTime"
     * in this connector.
     */
    std::string lastAccessedLogRecordTimeStr;

    //Parameters for Sqlite
    sqlite3 *db;
    sqlite3_stmt *selectStmt;
    sqlite3_stmt *deleteLogStmt;

    //The flag is true if there are new records in the log table.
    bool logRecordTimeChangedFlag;

    //Connect to the sqlite database
    bool connectToDB();
    //Check the config validity. e.g. if contains dbname, tables etc.
    bool checkConfigValidity();
    //Check if database contains the table.
    bool checkTableExistence();
    //Fetch the table schema and store into tableSchema
    bool populateTableSchema();

    //Create prepared statements for the listener.
    bool createPreparedStatement();
    //Create triggers for the log table
    bool createTriggerIfNotExistence();
    //Create the log table
    bool createLogTableIfNotExistence();

    //Load the lastAccessedLogRecordTime from the disk
    void loadLastAccessedLogRecordTime();

    //Delete the processed log from the table so that we can keep it small
    bool deleteProcessedLog();
};

#endif /* __SQLITECONNECTOR_H__ */
