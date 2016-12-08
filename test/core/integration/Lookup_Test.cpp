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
#include <instantsearch/Analyzer.h>
#include <instantsearch/Indexer.h>
#include <instantsearch/Term.h>
#include <instantsearch/Record.h>

#include "IntegrationTestHelper.h"
#include "analyzer/StandardAnalyzer.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <cstdlib>

#include <time.h>

using namespace std;

namespace srch2is = srch2::instantsearch;
using namespace srch2is;

unsigned mergeEveryNSeconds = 2;
unsigned mergeEveryMWrites = 5;
unsigned updateHistogramEveryPMerges = 1;
unsigned updateHistogramEveryQWrites = 5;

// Read data from file, build the index, and save the index to disk
void buildIndex(string data_file, string index_dir)
{
    /// Set up the Schema
    Schema *schema = Schema::create(srch2is::DefaultIndex);
    schema->setPrimaryKey("primaryKey");
    schema->setSearchableAttribute("description", 2);
    schema->setScoringExpression("idf_score*doc_boost");

    /// Create an Analyzer
    Analyzer *analyzer = new Analyzer(NULL, NULL, NULL, NULL, "", srch2is::STANDARD_ANALYZER);

    /// Create an index writer
    IndexMetaData *indexMetaData = new IndexMetaData( new CacheManager(),
    		mergeEveryNSeconds, mergeEveryMWrites,
    		updateHistogramEveryPMerges, updateHistogramEveryQWrites,
    		index_dir);
    Indexer *indexer = Indexer::create(indexMetaData, analyzer, schema);

    Record *record = new Record(schema);

    unsigned docsCounter = 0;
    string line;

    ifstream data(data_file.c_str());

    string recordToLookup = "";

    /// Read records from file
    /// the file should have two fields, seperated by '^'
    /// the first field is the primary key, the second field is a searchable attribute
    while(getline(data,line))
    {
        unsigned cellCounter = 0;
        stringstream  lineStream(line);
        string cell;

        while(getline(lineStream,cell,'^') && cellCounter < 3 )
        {
            if (cellCounter == 0)
            {
                record->setPrimaryKey(cell.c_str());

                if (recordToLookup == "")
                    recordToLookup = cell;
            }
            else if (cellCounter == 1)
            {
                record->setSearchableAttributeValue(0, cell);
            }
            else
            {
                float recordBoost = atof(cell.c_str());
                record->setRecordBoost(recordBoost);
            }

            cellCounter++;
        }

        indexer->addRecord(record, analyzer);

        docsCounter++;

        record->clear();
    }

    cout << "#Docs Read:" << docsCounter << endl;

    indexer->commit();
    indexer->save();

    cout << "Index saved." << endl;

    // test looking up a record in the index 
    ASSERT(indexer->lookupRecord(recordToLookup) == LU_PRESENT_IN_READVIEW_AND_WRITEVIEW);

    data.close();

    delete indexer;
    delete indexMetaData;
    delete analyzer;
    delete schema;
}

// Read data from file, update the index
void updateIndexAndLookupRecord(string data_file, Indexer *index)
{
    /// Set up the Schema
    Schema *schema = Schema::create(srch2is::DefaultIndex);
    schema->setPrimaryKey("primaryKey");
    schema->setSearchableAttribute("description", 2);

    Record *record = new Record(schema);

    unsigned docsCounter = 0;
    string line;

    ifstream data(data_file.c_str());

    string recordToLookup;

    /// Read one record from file to insert
    /// the file should have two fields, seperated by '^'
    /// the first field is the primary key, the second field is a searchable attribute
    if ( getline(data,line) )
    {
        unsigned cellCounter = 0;
        stringstream  lineStream(line);
        string cell;

        while(getline(lineStream,cell,'^') && cellCounter < 3 )
        {
            if (cellCounter == 0)
            {
                record->setPrimaryKey(cell.c_str());

                recordToLookup = cell;

                // test looking up a record that doesn't exist
                ASSERT(index->lookupRecord(recordToLookup) == LU_ABSENT_OR_TO_BE_DELETED);
            }
            else if (cellCounter == 1)
            {
                record->setSearchableAttributeValue(0, cell);
            }
            else
            {
                float recordBoost = atof(cell.c_str());
                record->setRecordBoost(recordBoost);
            }

            cellCounter++;
        }
        Analyzer* analyzer = getAnalyzer();
        index->addRecord(record, analyzer);
        delete analyzer;

        // test looking up a record that was just inserted and NOT merged yet
        ASSERT(index->lookupRecord(recordToLookup) == LU_TO_BE_INSERTED);

        docsCounter++;

        record->clear();
    }

    cout << "#New Docs Inserted:" << docsCounter << endl;

    sleep(11);

    // test looking up a record that was merged
    ASSERT(index->lookupRecord(recordToLookup) == LU_PRESENT_IN_READVIEW_AND_WRITEVIEW);

    cout << "Index updated." << endl;

    data.close();

    delete schema;
}

int main(int argc, char **argv)
{
    cout << "Test begins." << endl;
    cout << "-------------------------------------------------------" << endl;

    string index_dir = getenv("index_dir");
    string init_data_file = index_dir + "/init_data";
    string update_data_file = index_dir + "/update_data";

    cout << "Read init data from " << init_data_file << endl;
    cout << "Save index to " << index_dir << endl;
    cout << "Read update data from " << update_data_file << endl;

    buildIndex(init_data_file, index_dir);

    IndexMetaData *indexMetaData = new IndexMetaData( new CacheManager(),
    		mergeEveryNSeconds, mergeEveryMWrites,
    		updateHistogramEveryPMerges, updateHistogramEveryQWrites,
    		index_dir);
    Indexer *index = Indexer::load(indexMetaData);
    index->createAndStartMergeThreadLoop();
    cout << "Index loaded." << endl;

    updateIndexAndLookupRecord(update_data_file, index);
    
    delete index;
    delete indexMetaData;

    return 0;
}
