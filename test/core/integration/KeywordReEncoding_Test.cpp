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

//
// This test is to test the engine's capabilities to reassign ids to keywords when
// we cannot assign an integer id to a new keyword based on the ids of its adjacent
// neighbors.
//

#include <instantsearch/Analyzer.h>
#include "operation/IndexerInternal.h"
#include <instantsearch/QueryEvaluator.h>
#include <instantsearch/Query.h>
#include <instantsearch/Term.h>
#include <instantsearch/Schema.h>
#include <instantsearch/Record.h>
#include <instantsearch/QueryResults.h>
#include "util/Assert.h"
#include "util/encoding.h"
#include "IntegrationTestHelper.h"
#include "analyzer/StandardAnalyzer.h"
#include "analyzer/SimpleAnalyzer.h"
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>
#include "util/mypthread.h"

using namespace std;
using std::cout;
using std::endl;

using namespace std;
namespace srch2is = srch2::instantsearch;
using namespace srch2is;

string INDEX_DIR = getenv("index_dir");

void addSimpleRecords()
{
    ///Create Schema
    srch2is::Schema *schema = srch2is::Schema::create(srch2is::DefaultIndex);
    schema->setPrimaryKey("article_id"); // integer, not searchable
    schema->setSearchableAttribute("article_id"); // convert id to searchable text
    schema->setSearchableAttribute("article_authors", 2); // searchable text
    schema->setSearchableAttribute("article_title", 7); // searchable text

    Record *record = new Record(schema);

    Analyzer *analyzer = new Analyzer(NULL, NULL, NULL, NULL, "");
    // Create an index writer
    unsigned mergeEveryNSeconds = 3;    
    unsigned mergeEveryMWrites = 5;
    unsigned updateHistogramEveryPMerges = 1;
    unsigned updateHistogramEveryQWrites = 5;
    IndexMetaData *indexMetaData1 = new IndexMetaData( new CacheManager(),
    		mergeEveryNSeconds, mergeEveryMWrites,
    		updateHistogramEveryPMerges, updateHistogramEveryQWrites,
    		INDEX_DIR);
           
    Indexer *index = Indexer::create(indexMetaData1, analyzer, schema);
    
    record->setPrimaryKey(1001);
    record->setSearchableAttributeValue("article_authors", "Tom Smith and Jack Lennon");
    record->setSearchableAttributeValue("article_title", "Come Yesterday Once More");
    record->setRecordBoost(90);
    index->addRecord(record, analyzer);

    record->clear();
    record->setPrimaryKey(1002);
    record->setSearchableAttributeValue(1, "Jimi Hendrix");
    record->setSearchableAttributeValue(2, "Little wing");
    record->setRecordBoost(90);
    index->addRecord(record, analyzer);

    record->clear();
    record->setPrimaryKey(1003);
    record->setSearchableAttributeValue(1, "Tom Smith and Jack The Ripper");
    record->setSearchableAttributeValue(2, "Come Tomorrow Two More");
    record->setRecordBoost(10);
    index->addRecord(record, analyzer);

    index->commit();
    index->save();

    delete schema;
    delete record;
    delete analyzer;
    delete index;
}


#define MAX_THREAD 1000

typedef struct
{
  int id;
  int nproc;
} parm;

Indexer *indexer;

//char message[100];    /* storage for message  */
pthread_mutex_t msg_mutex = PTHREAD_MUTEX_INITIALIZER;
int token = 0;

//Create Index "A". Deserialise "A". Update Index "A". Search "A". Serialize "A"
void testRead(Indexer *indexer)
{
    // Create an index writer
    QueryEvaluatorRuntimeParametersContainer runtimeParameters;
    QueryEvaluator * queryEvaluator = new QueryEvaluator(indexer, &runtimeParameters);
    const Analyzer *analyzer =getAnalyzer();

    //Query: "tom", hits -> 1001, 1003
    {
        vector<unsigned> recordIds;
        recordIds.push_back(1001);
        recordIds.push_back(1003);
        //ASSERT ( ping(analyzer, queryEvaluator, "tom" , 2 , recordIds) == true);
        ping(analyzer, queryEvaluator, "tom" , 2 , recordIds, vector<unsigned>(), ATTRIBUTES_OP_AND);
    }
    //Query: "jimi", hit -> 1002
    {
        vector<unsigned> recordIds;
        recordIds.push_back(1002);
        //ASSERT ( ping(analyzer, queryEvaluator, "jimi" , 1 , recordIds) == true);
        ping(analyzer, queryEvaluator, "jimi" , 1 , recordIds, vector<unsigned>(), ATTRIBUTES_OP_AND);
    }

    //Query: "smith", hits -> 1001, 1003
    {
        vector<unsigned> recordIds;
        recordIds.push_back(1001);
        recordIds.push_back(1003);
        //ASSERT ( ping(analyzer, queryEvaluator, "smith" , 2 , recordIds) == true);
        ping(analyzer, queryEvaluator, "smith" , 2 , recordIds, vector<unsigned>(), ATTRIBUTES_OP_AND);
    }

    //Query: "jobs", hit -> 1002
    {
        vector<unsigned> recordIds;
        recordIds.push_back(1999);
        //ASSERT ( ping(analyzer, queryEvaluator, "jobs" , 1 , recordIds) == true);
        ping(analyzer, queryEvaluator, "jobs" , 1 , recordIds, vector<unsigned>(), ATTRIBUTES_OP_AND);
    }

    //indexer->print_index();

    //Query: "smith", hits -> 1001, 1003
    {
        vector<unsigned> recordIds;
        recordIds.push_back(1001);
        recordIds.push_back(1003);
        //ASSERT ( ping(analyzer, queryEvaluator, "smith" , 2 , recordIds) == true);
        ping(analyzer, queryEvaluator, "smith" , 2 , recordIds, vector<unsigned>(), ATTRIBUTES_OP_AND);
    }

    //Query: "jobs", hit -> 1999
    {
        vector<unsigned> recordIds;
        recordIds.push_back(1999);
        //ASSERT ( ping(analyzer, queryEvaluator, "jobs" , 1 , recordIds) == true);
        ping(analyzer, queryEvaluator, "jobs" , 1 , recordIds, vector<unsigned>(), ATTRIBUTES_OP_AND);
    }

    //Query: "smith", hits -> 1002, 1998
    {
        vector<unsigned> recordIds;
        recordIds.push_back(1998);
        recordIds.push_back(1002);
        //ASSERT ( ping(analyzer, queryEvaluator, "jimi" , 2 , recordIds) == true);
        ping(analyzer, queryEvaluator, "jimi" , 2 , recordIds, vector<unsigned>(), ATTRIBUTES_OP_AND);
    }

    //Query: "jobs", hits -> 1998 , 1999
    {
        vector<unsigned> recordIds;
        recordIds.push_back(1998);
        recordIds.push_back(1999);
        //ASSERT ( ping(analyzer, queryEvaluator, "jobs" , 2 , recordIds) == true);
        ping(analyzer, queryEvaluator, "jobs" , 2 , recordIds, vector<unsigned>(), ATTRIBUTES_OP_AND);
    }

    //Query: "tom", hits -> 1001, 1003 , 1999
    {
        vector<unsigned> recordIds;
        recordIds.push_back(1001);
        recordIds.push_back(1999);
        recordIds.push_back(1003);
        //ASSERT ( ping(analyzer, queryEvaluator, "tom" , 3 , recordIds) == true);
        ping(analyzer, queryEvaluator, "tom" , 3 , recordIds, vector<unsigned>(), ATTRIBUTES_OP_AND);
    }

    delete queryEvaluator;
    delete analyzer;

}

void* writerUsingSimilarKeywords(void *arg)
{
    pthread_mutex_lock(&msg_mutex);

    // we insert a lot of similar keywords to produce the case where one of them cannot be 
    // assigned an order-preserving integer
    for (unsigned i = 1; i <= 40; i ++) {
        Record *record = new Record(indexer->getSchema());
        unsigned recordPrimaryKey = i;

        record->setPrimaryKey(recordPrimaryKey);

        // http://stackoverflow.com/questions/191757/c-concatenate-string-and-int
        char numstr[21]; // enough to hold all numbers up to 64-bits
        sprintf(numstr, "%d", i);
        // form a string with three keywords, while the 2nd one is in the middle and can "increase"
        // For example, "aaa0 aaa8 aaaz"
        string aString = "aaa";
        string string1 = aString + "0 " + aString + numstr + " aaaz"; 
        string bString = "bbb";
        string string2 = bString + "0 " + bString + numstr + " bbbz";

        //keep adding to the left side
        string string3 = "";
        for (unsigned j = i; j <= 40; j++) {
            string3 += "0";
        }

        //keep adding to the right side
        string string4 = "";
        for (unsigned j = 0; j < i; j++) {
            string4 += "z";
        }

        //record->setSearchableAttributeValue(1, string1);
        //record->setSearchableAttributeValue(2, string1);
        //cout << "add keywords:\nstring1 = " << string1 << ", string2 = " << string2 << endl;
        record->setSearchableAttributeValue(1, string3);
        record->setSearchableAttributeValue(2, string4);
        //cout << "add keywords:\nstring1 = " << string3 << ", string2 = " << string4 << endl;
        //record->setSearchableAttributeValue(1, string1 + " " + string2);
        //record->setSearchableAttributeValue(2, string3 + " " + string4);

        record->setRecordBoost(90);
        Analyzer *analyzer = getAnalyzer();

        indexer->addRecord(record, analyzer);
        indexer->merge_ForTesting();
    
        vector<unsigned> recordIds;
        recordIds.push_back(recordPrimaryKey);
        QueryEvaluatorRuntimeParametersContainer runtimeParameters;
        QueryEvaluator * queryEvaluator = new QueryEvaluator(indexer, &runtimeParameters);
        ping(analyzer, queryEvaluator, aString + numstr, 1 , recordIds, vector<unsigned>(), ATTRIBUTES_OP_AND);
        delete analyzer;
    }

    pthread_mutex_unlock(&msg_mutex);
    return NULL;
}

void* reader(void *arg)
{
    testRead(indexer);
    //sleep(2);
    testRead(indexer);
    return NULL;
}

// Multiple readers, single writer
void test1()
{
    pthread_t *threadReaders;
    pthread_t threadWriter;

    pthread_attr_t pthread_custom_attr;
    parm *p;
    int threadNumber;

    addSimpleRecords();

    // create an indexer
    unsigned mergeEveryNSeconds = 2;
    unsigned mergeEveryMWrites = 5;
    unsigned updateHistogramEveryPMerges = 1;
    unsigned updateHistogramEveryQWrites = 5;
    IndexMetaData *indexMetaData1 = new IndexMetaData( new CacheManager(),
    		mergeEveryNSeconds, mergeEveryMWrites,
    		updateHistogramEveryPMerges, updateHistogramEveryQWrites,
    		INDEX_DIR);
    indexer = Indexer::load(indexMetaData1);

    //threadNumber = 1000;
    threadNumber = 1;
    threadReaders = (pthread_t *) malloc((threadNumber-1) * sizeof(*threadReaders)); // n-1 readers
    pthread_attr_init(&pthread_custom_attr);
    p = (parm *)malloc(sizeof(parm) * threadNumber);
  
    // Start n - 1 readers and 1 writer
    for (int i = 0; i < threadNumber; i++) {
        p[i].id = i;
        p[i].nproc = threadNumber;
        if ( i < threadNumber - 1)
            pthread_create(&threadReaders[i], &pthread_custom_attr, reader, (void *)(p+i));
        else
            pthread_create(&threadWriter, &pthread_custom_attr, writerUsingSimilarKeywords, (void *)(p+i));
    }
  
    // Synchronize the completion of each thread.
    for (int i = 0; i < threadNumber; i++) {
        if (i < threadNumber - 1)
            pthread_join(threadReaders[i], NULL);
        else
            pthread_join(threadWriter, NULL);
    }
  
    free(p);
    delete indexer;
}

//SimpleAnalyzer organizes a tokenizer using " " as the delimiter and a "ToLowerCase" filter
void testAnalyzer1()
{
	string src="We are美丽 Chinese";
	AnalyzerInternal *simpleAnlyzer = new SimpleAnalyzer(NULL, NULL, NULL, NULL, "");
	TokenStream * tokenStream = simpleAnlyzer->createOperatorFlow();
	tokenStream->fillInCharacters(src);
	vector<string> vectorString;
	vectorString.push_back("we");
	vectorString.push_back("are美丽");
	vectorString.push_back("chinese");
	int i=0;
	while(tokenStream->processToken())
	{
		vector<CharType> charVector;
		charVector = tokenStream->getProcessedToken();
		charTypeVectorToUtf8String(charVector, src);
		ASSERT(vectorString[i] == src);
		i++;
	}
	delete tokenStream;
	delete simpleAnlyzer;
}
//StandardAnalyzer organizes a tokenizer treating characters >= 256 as a single token and   a "ToLowerCase" filter
void testAnalyzer2()
{
	string src="We are美丽 Chineseㄓㄠ";
	AnalyzerInternal *standardAnalyzer = new StandardAnalyzer(NULL, NULL, NULL, NULL, "");
	TokenStream * tokenStream = standardAnalyzer->createOperatorFlow();
	tokenStream->fillInCharacters(src);
	vector<string> vectorString;
	vectorString.push_back("we");
	vectorString.push_back("are");
	vectorString.push_back("美");
	vectorString.push_back("丽");
	vectorString.push_back("chinese");
	vectorString.push_back("ㄓㄠ");
	int i=0;
	while(tokenStream->processToken())
	{
		vector<CharType> charVector;
		charVector = tokenStream->getProcessedToken();
		charTypeVectorToUtf8String(charVector, src);
		ASSERT(vectorString[i] == src);
		i++;
	}
	delete tokenStream;
	delete standardAnalyzer;
}
int main(int argc, char *argv[])
{
	//testAnalyzer1();
	cout << "SimpleAnalyzer test passed" << endl;

	testAnalyzer2();
	cout << "StandardAnalyzer test passed" << endl;

    // multiple readers and one writer
	cout << "Multiple Readers and One writer." << endl;
	test1();
	cout << "Test passed" << endl;

    return 0;
}
