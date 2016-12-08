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
 * mysqlEventHandler.cpp
 *
 *  Created on: Sep 17, 2014
 *
 *  This piece of code is modified from the web site: (mysql-replication-listener example code)
 *  http://bazaar.launchpad.net/~mysql/mysql-replication-listener/trunk/files/head:/examples/mysql2lucene/
 */

#include "mysqlEventHandler.h"
#include "json/json.h"
#include "Logger.h"

using srch2::util::Logger;

/****************************MySQLTableIndex*******************************/
//Table index populates the table id and table name.
mysql::Binary_log_event *MySQLTableIndex::process_event(
        mysql::Table_map_event *tm) {
    if (find(tm->table_id) == end())
        insert(std::pair<uint64_t, mysql::Table_map_event *>(tm->table_id, tm));

    /* Consume this event so it won't be captured by other handlers */
    return 0;
}

MySQLTableIndex::~MySQLTableIndex() {
    Int2event_map::iterator it = begin();
    do {
        delete it->second;
    } while (++it != end());
}

/************************MySQLIncidentHandler******************************/
//This class handles all the incident events like LOST_EVENTS.
mysql::Binary_log_event * MySQLIncidentHandler::process_event(
        mysql::Incident_event * incident) {
    Logger::debug("MYSQLCONNECTOR: Incident event type: %s\n length: %d,"
            " next pos: %d\n type= %u, message= %s",
            mysql::system::get_event_type_str(incident->get_event_type()),
            incident->header()->event_length, incident->header()->next_position,
            (unsigned) incident->type, incident->message.c_str());
    /* Consume the event */
    delete incident;
    return 0;
}

/*****************************MySQLApplier**********************************/
//This class handles insert, delete, update events.
MySQLApplier::MySQLApplier(MySQLTableIndex * index,
        ServerInterface * serverInterface, std::vector<std::string> * schemaName,
        time_t & startTimestamp, std::string & pk) {
    tableIndex = index;
    this->serverInterface = serverInterface;
    this->schemaName = schemaName;
    this->startTimestamp = startTimestamp;
    this->pk = pk;
}

mysql::Binary_log_event * MySQLApplier::process_event(mysql::Row_event * rev) {
    std::string expectedTableName, lowerTableName;
    this->serverInterface->configLookUp("tableName", expectedTableName);

    time_t ts = rev->header()->timestamp;

    //Ignore the event that earlier than the 'startTimestamp'.
    if (ts < startTimestamp) {
        return rev;
    }

    startTimestamp = ts;    //Update the time stamp

    //Get the table id
    uint64_t table_id = rev->table_id;
    Int2event_map::iterator ti_it = tableIndex->find(table_id);
    if (ti_it == tableIndex->end()) {
        Logger::error("MYSQLCONNECTOR: Table id %d was not registered"
                " by any preceding table map event.");
        return rev;
    }

    /*
     * Transform expectedTableName and this row event table name to lower case,
     * because on MAC, mysql table name is case insensitive.
     */
    lowerTableName = ti_it->second->table_name;
    std::transform(expectedTableName.begin(), expectedTableName.end(),
            expectedTableName.begin(), ::tolower);
    std::transform(lowerTableName.begin(), lowerTableName.end(),
            lowerTableName.begin(), ::tolower);

    //Ignore all other tables
    if (expectedTableName.compare(lowerTableName) != 0) {
        return rev;
    }

    /*
     * Each row event contains multiple rows and fields. The Row_iterator
     * allows us to iterate one row at a time.
     */
    mysql::Row_event_set rows(rev, ti_it->second);

    //Create a fully qualified table name
    std::ostringstream os;
    os << ti_it->second->db_name << '.' << ti_it->second->table_name;
    std::string fullName = os.str();
    try {
        mysql::Row_event_set::iterator it = rows.begin();
        do {
            mysql::Row_of_fields fields = *it;
            if (rev->get_event_type() == mysql::WRITE_ROWS_EVENT
                    || rev->get_event_type() == mysql::WRITE_ROWS_EVENT_V1) {
                tableInsert(fullName, fields);
            }

            if (rev->get_event_type() == mysql::UPDATE_ROWS_EVENT
                    || rev->get_event_type() == mysql::UPDATE_ROWS_EVENT_V1) {
                ++it;
                mysql::Row_of_fields fields2 = *it;
                tableUpdate(fullName, fields, fields2);
            }
            if (rev->get_event_type() == mysql::DELETE_ROWS_EVENT
                    || rev->get_event_type() == mysql::DELETE_ROWS_EVENT_V1) {
                tableDelete(fullName, fields);
            }

        } while (++it != rows.end());
    } catch (const std::logic_error& le) {
        Logger::error("MYSQLCONNECTOR: MySQL data type error : %s", le.what());
    }

    return rev;
}

void MySQLApplier::tableInsert(std::string & table_name,
        mysql::Row_of_fields &fields) {
    mysql::Row_of_fields::iterator field_it = fields.begin();
    std::vector<std::string>::iterator schema_it = schemaName->begin();
    mysql::Converter converter;

    Json::Value record;
    Json::FastWriter writer;

    do {
        /*
         Each row contains a vector of Value objects. The converter
         allows us to transform the value into another
         representation.
         */
        std::string str;
        converter.to(str, *field_it);

        record[*schema_it] = str;

        field_it++;
        schema_it++;
    } while (field_it != fields.end() && schema_it != schemaName->end());
    std::string jsonString = writer.write(record);
    Logger::debug("MYSQLCONNECTOR: Inserting %s ", jsonString.c_str());

    serverInterface->insertRecord(jsonString);
}

void MySQLApplier::tableDelete(std::string & table_name,
        mysql::Row_of_fields &fields) {
    mysql::Row_of_fields::iterator field_it = fields.begin();
    std::vector<std::string>::iterator schema_it = schemaName->begin();
    mysql::Converter converter;
    do {
        /*
         Each row contains a vector of Value objects. The converter
         allows us to transform the value into another
         representation.
         */

        std::string str;
        converter.to(str, *field_it);
        if ((*schema_it).compare(this->pk.c_str()) == 0) {
            Logger::debug("MYSQLCONNECTOR: Deleting primary key %s = %s ",
                    this->pk.c_str(), str.c_str());
            serverInterface->deleteRecord(str);
            break;
        }
        field_it++;
        schema_it++;
    } while (field_it != fields.end() && schema_it != schemaName->end());
    if (field_it == fields.end() || schema_it == schemaName->end()) {
        Logger::error("MYSQLCONNECTOR: Schema doesn't contain primary key.");
    }
}

void MySQLApplier::tableUpdate(std::string & table_name,
        mysql::Row_of_fields &old_fields, mysql::Row_of_fields &new_fields) {
    /*
     Find previous entry and delete it.
     */
    tableDelete(table_name, old_fields);

    /*
     Insert new entry.
     */
    tableInsert(table_name, new_fields);

}

