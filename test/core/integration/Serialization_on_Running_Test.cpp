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
#include <instantsearch/QueryEvaluator.h>
#include <instantsearch/Query.h>
#include <instantsearch/Term.h>
#include <instantsearch/QueryResults.h>
#include "IntegrationTestHelper.h"
#include "MapSearchTestHelper.h"
#include "analyzer/StandardAnalyzer.h"

#include <time.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdlib>

#define MAX_QUERY_NUMBER 5000

using namespace std;

namespace srch2is = srch2::instantsearch;
using namespace srch2is;

unsigned mergeEveryNSeconds = 1;
unsigned mergeEveryMWrites = 5;
unsigned updateHistogramEveryPMerges = 1;
unsigned updateHistogramEveryQWrites = 5;

Indexer *buildIndex(string data_file, string index_dir, string expression, vector<pair<string, string> > &records_in_index)
{
    /// Set up the Schema
    Schema *schema = Schema::create(srch2is::DefaultIndex);
    schema->setPrimaryKey("id");
    schema->setSearchableAttribute("name", 2);
    schema->setSearchableAttribute("category", 1);
    schema->setScoringExpression(expression);

    /// Create an Analyzer
    Analyzer *analyzer = new Analyzer(NULL, NULL, NULL, NULL, "",
                                      srch2is::STANDARD_ANALYZER);

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

    /// Read records from file
    /// the file should have two fields, seperated by '^'
    /// the first field is the primary key, the second field is a searchable attribute
    while(getline(data,line))
    {
        unsigned cellCounter = 0;
        stringstream  lineStream(line);
        string cell;

        pair<string, string> newp;
        while(getline(lineStream,cell,'^') && cellCounter < 4 )
        {
            if (cellCounter == 0)
            {
                newp.first = cell;
                record->setPrimaryKey(cell.c_str());
            }
            else if (cellCounter == 1)
            {
                newp.second = cell;
                record->setSearchableAttributeValue(0, cell);
            }
            else if (cellCounter == 2)
            {
                newp.second += " " + cell;
                record->setSearchableAttributeValue(1, cell);
            }
            else if (cellCounter == 3)
            {
                record->setRecordBoost(atof(cell.c_str()));
            }

            cellCounter++;
        }

        records_in_index.push_back(newp);

        indexer->addRecord(record, analyzer);

        docsCounter++;

        record->clear();
    }

    cout << "#Docs Read:" << docsCounter << endl;

    indexer->commit();

    data.close();

    return indexer;
}

void updateAndSaveIndex(Indexer *indexer, Analyzer* analyzer, string data_file, vector<pair<string, string> > &records_in_index)
{
    Record *record = new Record(indexer->getSchema());

    unsigned docsCounter = 0;
    string line;

    ifstream data(data_file.c_str());

    /// Read records from file
    /// the file should have two fields, seperated by '^'
    /// the first field is the primary key, the second field is a searchable attribute
    while(getline(data,line))
    {
        unsigned cellCounter = 0;
        stringstream  lineStream(line);
        string cell;

        pair<string, string> newp;
        while(getline(lineStream,cell,'^') && cellCounter < 4 )
        {
            if (cellCounter == 0)
            {
                newp.first = cell;
                record->setPrimaryKey(cell.c_str());
            }
            else if (cellCounter == 1)
            {
                newp.second = cell;
                record->setSearchableAttributeValue(0, cell);
            }
            else if (cellCounter == 2)
            {
                newp.second += " " + cell;
                record->setSearchableAttributeValue(1, cell);
            }
            else if (cellCounter == 3)
            {
                record->setRecordBoost(atof(cell.c_str()));
            }

            cellCounter++;
        }

        records_in_index.push_back(newp);

        indexer->addRecord(record, analyzer);

        docsCounter++;

        record->clear();
    }

    cout << "#Docs Added:" << docsCounter << endl;

    indexer->save();

    data.close();
}

Indexer *buildGeoIndex(string data_file, string index_dir, string expression, vector<pair<pair<string, Point>, string> > &records_in_index)
{
    /// Set up the Schema
    Schema *schema = Schema::create(srch2is::LocationIndex);
    schema->setPrimaryKey("id");
    schema->setSearchableAttribute("name", 2);
    schema->setSearchableAttribute("category", 1);
    schema->setScoringExpression(expression);

    /// Create an Analyzer
    Analyzer *analyzer = new Analyzer(NULL, NULL, NULL, NULL, "",
                                      srch2is::STANDARD_ANALYZER);

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

    /// Read records from file
    /// the file should have two fields, seperated by '^'
    /// the first field is the primary key, the second field is a searchable attribute
    while(getline(data,line))
    {
        unsigned cellCounter = 0;
        stringstream  lineStream(line);
        string cell;
        float lat=0.0, lng=0.0;

        pair<pair<string, Point>, string> newp;
        while(getline(lineStream,cell,'^') && cellCounter < 6 )
        {
            if (cellCounter == 0)
            {
                newp.first.first = cell;
                record->setPrimaryKey(cell.c_str());
            }
            else if (cellCounter == 1)
            {
                newp.second = cell;
                record->setSearchableAttributeValue(0, cell);
            }
            else if (cellCounter == 2)
            {
                newp.second += " " + cell;
                record->setSearchableAttributeValue(1, cell);
            }
            else if (cellCounter == 3)
            {
                record->setRecordBoost(atof(cell.c_str()));
            }
            else if (cellCounter == 4)
            {
                lat = atof(cell.c_str());
            }
            else if (cellCounter == 5)
            {
                lng = atof(cell.c_str());
            }

            cellCounter++;
        }

        newp.first.second.x = lat;
        newp.first.second.y = lng;
        records_in_index.push_back(newp);

        record->setLocationAttributeValue(lat, lng);

        indexer->addRecord(record, analyzer);

        docsCounter++;

        record->clear();
    }

    cout << "#Docs Read:" << docsCounter << endl;

    indexer->commit();

    data.close();

    return indexer;
}

void updateAndSaveGeoIndex(Indexer *indexer,Analyzer* analyzer, string data_file, vector<pair<pair<string, Point>, string> > &records_in_index)
{
    Record *record = new Record(indexer->getSchema());

    unsigned docsCounter = 0;
    string line;

    ifstream data(data_file.c_str());

    /// Read records from file
    /// the file should have two fields, seperated by '^'
    /// the first field is the primary key, the second field is a searchable attribute
    while(getline(data,line))
    {
        unsigned cellCounter = 0;
        stringstream  lineStream(line);
        string cell;
        float lat=0.0, lng=0.0;

        pair<pair<string, Point>, string> newp;
        while(getline(lineStream,cell,'^') && cellCounter < 6 )
        {
            if (cellCounter == 0)
            {
                newp.first.first = cell;
                record->setPrimaryKey(cell.c_str());
            }
            else if (cellCounter == 1)
            {
                newp.second = cell;
                record->setSearchableAttributeValue(0, cell);
            }
            else if (cellCounter == 2)
            {
                newp.second += " " + cell;
                record->setSearchableAttributeValue(1, cell);
            }
            else if (cellCounter == 3)
            {
                record->setRecordBoost(atof(cell.c_str()));
            }
            else if (cellCounter == 4)
            {
                lat = atof(cell.c_str());
            }
            else if (cellCounter == 5)
            {
                lng = atof(cell.c_str());
            }

            cellCounter++;
        }

        newp.first.second.x = lat;
        newp.first.second.y = lng;
        records_in_index.push_back(newp);

        record->setLocationAttributeValue(lat, lng);

        indexer->addRecord(record, analyzer);

        docsCounter++;

        record->clear();
    }

    cout << "#Docs added:" << docsCounter << endl;

    indexer->save();

    data.close();
}

void validateDefaultIndex(const Analyzer *analyzer, QueryEvaluator *queryEvaluator, vector<pair<string, string> > &records_in_index)
{
    int k = 10;

    for (int i=0; i<records_in_index.size(); i++)
    {
        bool ifFound = false;
        ifFound = existsInTopK(analyzer, queryEvaluator, records_in_index[i].second, records_in_index[i].first, k);
        ASSERT(ifFound);
    }

}

void validateGeoIndex(const Analyzer *analyzer, QueryEvaluator *  queryEvaluator, vector<pair<pair<string, Point>, string> > &records_in_index)
{
    int k = 10;

    for (int i=0; i<records_in_index.size(); i++)
    {
        bool ifFound = false;
        float lb_lat = records_in_index[i].first.second.x - 0.5;
        float lb_lng = records_in_index[i].first.second.y - 0.5;
        float rt_lat = records_in_index[i].first.second.x + 0.5;
        float rt_lng = records_in_index[i].first.second.y + 0.5;
        ifFound = existsInTopKGeo(analyzer, queryEvaluator, records_in_index[i].second, records_in_index[i].first.first, k, lb_lat, lb_lng, rt_lat, rt_lng);
        ASSERT(ifFound);
    }

}

void testDefaultIndex(string index_dir)
{
    vector<pair<string, string> > records_in_index;

    // load and validate the initial index

    Indexer *indexer = buildIndex(index_dir+"/data/init", index_dir, "idf_score*doc_boost", records_in_index);

    QueryEvaluatorRuntimeParametersContainer runtimeParameters;
    QueryEvaluator * queryEvaluator = new QueryEvaluator(indexer, &runtimeParameters);

    Analyzer *analyzer = getAnalyzer();

    validateDefaultIndex(analyzer, queryEvaluator, records_in_index);
    cout << "Init Default Index Validated." << endl;

    // update the index and serialize it

    updateAndSaveIndex(indexer, analyzer, index_dir+"/data/update", records_in_index);
    sleep(mergeEveryNSeconds + 1);

    // load the index again and validate it

    CacheManager *cache = new CacheManager(134217728);
    IndexMetaData *indexMetaData = new IndexMetaData(cache,
    		mergeEveryNSeconds, mergeEveryMWrites,
    		updateHistogramEveryPMerges, updateHistogramEveryQWrites,
    		index_dir);

    Indexer *indexerLoaded = Indexer::load(indexMetaData);
    QueryEvaluator * queryEvaluatorLoaded = new QueryEvaluator(indexerLoaded, &runtimeParameters);

    Analyzer *analyzerLoaded = getAnalyzer();

    validateDefaultIndex(analyzerLoaded, queryEvaluatorLoaded, records_in_index);

    cout << "Loaded Default Index Validated." << endl;

    delete queryEvaluator;
    delete indexer;
    delete queryEvaluatorLoaded;
    delete indexerLoaded;

    cout << "Default Index Pass." << endl;
}

void testGeoIndex(string index_dir)
{
    vector<pair<pair<string, Point>, string> > records_in_index;

    // load and validate the initial index

    Indexer *indexer = buildGeoIndex(index_dir+"/data/init", index_dir, "idf_score*doc_boost", records_in_index);

    QueryEvaluatorRuntimeParametersContainer runTimeParameters;
    QueryEvaluator * queryEvaluator = new QueryEvaluator(indexer,&runTimeParameters );
    Analyzer *analyzer = getAnalyzer();

    validateGeoIndex(analyzer, queryEvaluator, records_in_index);
    cout << "Init Geo Index Validated." << endl;

    // update the index and serialize it

    updateAndSaveGeoIndex(indexer, analyzer, index_dir+"/data/update", records_in_index);
    sleep(mergeEveryNSeconds + 1);

    // load the index again and validate it

    CacheManager *cache = new CacheManager(134217728);
    IndexMetaData *indexMetaData = new IndexMetaData(cache,
    		mergeEveryNSeconds, mergeEveryMWrites,
    		updateHistogramEveryPMerges, updateHistogramEveryQWrites,
    		index_dir);

    Indexer *indexerLoaded = Indexer::load(indexMetaData);
    QueryEvaluator * queryEvaluatorLoaded = new QueryEvaluator(indexer,&runTimeParameters );
    Analyzer *analyzerLoaded = getAnalyzer();

    validateGeoIndex(analyzerLoaded, queryEvaluatorLoaded, records_in_index);

    cout << "Loaded Geo Index Validated." << endl;

    delete queryEvaluator;
    delete indexer;
    delete queryEvaluatorLoaded;
    delete indexerLoaded;

    cout << "Geo Index Pass." << endl;
}

int main(int argc, char **argv)
{
    cout << "Test begins." << endl;
    cout << "-------------------------------------------------------" << endl;

    string index_dir = getenv("index_dir");

    testDefaultIndex(index_dir);
    testGeoIndex(index_dir);

    return 0;
}
