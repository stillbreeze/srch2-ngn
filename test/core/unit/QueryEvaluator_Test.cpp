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
#include "operation/QueryEvaluatorInternal.h"
#include "operation/IndexerInternal.h"
#include "util/Assert.h"
#include "util/Logger.h"
#include "analyzer/AnalyzerInternal.h"
#include "analyzer/AnalyzerContainers.h"

#include <instantsearch/Term.h>
#include <instantsearch/Schema.h>
#include <instantsearch/Record.h>
#include <instantsearch/QueryResults.h>
#include <instantsearch/Indexer.h>
#include "UnitTestHelper.h"

#include <iostream>
#include <functional>
#include <vector>
#include <string>

using namespace std;
namespace srch2is = srch2::instantsearch;
using namespace srch2is;
using srch2::util::Logger;

typedef Trie Trie_Internal;

const char* INDEX_DIR = ".";

bool checkContainment(vector<string> &prefixVector, const string &prefix) {
    for (unsigned i = 0; i < prefixVector.size(); i++) {
        if (prefixVector.at(i) == prefix)
            return true;
    }

    return false;
}

// TODO Needs change in   indexSearcherInternal->computeActiveNodeSet(term) to run.
void ActiveNodeSet_test()
{
 /*   // construct a trie for the searcher
    Trie *trie = new Trie();

    unsigned invertedIndexOffset;
    trie->addKeyword("cancer", invertedIndexOffset);
    trie->addKeyword("canada", invertedIndexOffset);
    trie->addKeyword("canteen", invertedIndexOffset);
    trie->addKeyword("can", invertedIndexOffset);
    trie->addKeyword("cat", invertedIndexOffset);
    trie->addKeyword("dog", invertedIndexOffset);
    trie->commit();
*/

	///Create Schema
	Schema *schema = Schema::create(srch2::instantsearch::DefaultIndex);
	schema->setPrimaryKey("article_id"); // integer, not searchable
	schema->setSearchableAttribute("article_title", 7); // searchable text

	Record *record = new Record(schema);
        SynonymContainer *syn = SynonymContainer::getInstance("", SYNONYM_DONOT_KEEP_ORIGIN);
        syn->init();
	Analyzer *analyzer = new Analyzer(NULL, NULL, NULL, syn, "");

	unsigned mergeEveryNSeconds = 3;
	unsigned mergeEveryMWrites = 5;
    unsigned updateHistogramEveryPMerges = 1;
    unsigned updateHistogramEveryQWrites = 5;
	srch2is::IndexMetaData *indexMetaData = new srch2is::IndexMetaData(new CacheManager(134217728),
			mergeEveryNSeconds, mergeEveryMWrites,
			updateHistogramEveryPMerges, updateHistogramEveryQWrites,
			INDEX_DIR);
	srch2is::Indexer *indexer = srch2is::Indexer::create(indexMetaData, analyzer, schema);

	record->setPrimaryKey(1001);
	record->setSearchableAttributeValue("article_title", "cancer canada canteen can cat dog");
	string testRecord = "test string";
	record->setInMemoryData(testRecord.c_str(), testRecord.length());
	indexer->addRecord(record, analyzer);
	indexer->commit();

	 QueryEvaluatorRuntimeParametersContainer runTimeParameters(10000, 500, 500);

	QueryEvaluator * queryEvaluator = new QueryEvaluator(indexer, &runTimeParameters);
	queryEvaluator->impl->readerPreEnter();
    unsigned threshold = 2;
    Term *term = FuzzyTerm::create("nce", TERM_TYPE_PREFIX, 1, 1, threshold);
    PrefixActiveNodeSet *prefixActiveNodeSet = queryEvaluator->impl
            ->computeActiveNodeSet(term).get();
    vector<string> similarPrefixes;
    prefixActiveNodeSet->getComputedSimilarPrefixes(
            queryEvaluator->impl->testOnly_getTrie(), similarPrefixes);

    unsigned sim_size = similarPrefixes.size();

    (void) sim_size;

    ASSERT(similarPrefixes.size() == 2);
    ASSERT(checkContainment(similarPrefixes, "c"));
    //ASSERT(checkContainment(similarPrefixes, "ca"));
    ASSERT(checkContainment(similarPrefixes, "cance"));
    similarPrefixes.clear();

    // We don't need to delete prefixActiveNodeSet since it's cached and will be
    // deleted in the destructor of indexSearchInternal
    queryEvaluator->impl->readerPreExit();
    delete queryEvaluator;
    delete term;
    delete indexMetaData;
    delete indexer;
    syn->free();
}

/**
 * Keyword: Record
 *   Pink:  2, 5, 6
 *   Pinksyponzi: 4
 *
 *   Floyd: 2, 6
 *   Floydsyponzi: 4
 *
 *   Shine: 2, 3
 *   Shinesyponzi: 4
 */
void addRecords() {
    ///Create Schema
    Schema *schema = Schema::create(srch2::instantsearch::DefaultIndex);
    schema->setPrimaryKey("article_id"); // integer, not searchable
    schema->setSearchableAttribute("article_id"); // convert id to searchable text
    schema->setSearchableAttribute("article_authors", 2); // searchable text
    schema->setSearchableAttribute("article_title", 7); // searchable text

    Record *record = new Record(schema);
    SynonymContainer *syn = SynonymContainer::getInstance("", SYNONYM_DONOT_KEEP_ORIGIN);
    syn->init();
    Analyzer *analyzer = new Analyzer(NULL, NULL, NULL, syn, "");

    unsigned mergeEveryNSeconds = 3;
    unsigned mergeEveryMWrites = 5;
    unsigned updateHistogramEveryPMerges = 1;
    unsigned updateHistogramEveryQWrites = 5;
    srch2is::IndexMetaData *indexMetaData = new srch2is::IndexMetaData(NULL,
            mergeEveryNSeconds, mergeEveryMWrites,
            updateHistogramEveryPMerges, updateHistogramEveryQWrites,
            INDEX_DIR);
    srch2is::Indexer *index = srch2is::Indexer::create(indexMetaData, analyzer,
            schema);

    record->setPrimaryKey(1001);
    record->setSearchableAttributeValue("article_authors",
            "Tom Smith and Jack Lennon");
    record->setSearchableAttributeValue("article_title",
            "come Yesterday Once More");
    record->setRecordBoost(10);
	string testRecord = "test string";
	record->setInMemoryData(testRecord.c_str(), testRecord.length());
    index->addRecord(record, analyzer);

    record->clear();
    record->setPrimaryKey(1002);
    record->setSearchableAttributeValue(0, "George Harris");
    record->setSearchableAttributeValue(1, "Here comes the sun");
    record->setRecordBoost(20);
    record->setInMemoryData(testRecord.c_str(), testRecord.length());
    index->addRecord(record, analyzer);
    record->clear();
    record->setPrimaryKey(1003);
    record->setSearchableAttributeValue(0, "Pink Floyd");
    record->setSearchableAttributeValue(1, "Shine on you crazy diamond");
    record->setRecordBoost(30);
    record->setInMemoryData(testRecord.c_str(), testRecord.length());
    index->addRecord(record, analyzer);
    record->clear();
    record->setPrimaryKey(1004);
    record->setSearchableAttributeValue(0, "Uriah Hepp");
    record->setSearchableAttributeValue(1, "Come Shine away Melinda ");
    record->setRecordBoost(40);
    record->setInMemoryData(testRecord.c_str(), testRecord.length());
    index->addRecord(record, analyzer);
    record->clear();
    record->setPrimaryKey(1005);
    record->setSearchableAttributeValue(0, "Pinksyponzi Floydsyponzi");
    record->setSearchableAttributeValue(1,
            "Shinesyponzi on Wish you were here");
    record->setRecordBoost(50);
    record->setInMemoryData(testRecord.c_str(), testRecord.length());
    index->addRecord(record, analyzer);
    record->clear();
    record->setPrimaryKey(1006);
    record->setSearchableAttributeValue(0, "U2 2345 Pink");
    record->setSearchableAttributeValue(1, "with or without you");
    record->setRecordBoost(60);
    record->setInMemoryData(testRecord.c_str(), testRecord.length());
    index->addRecord(record, analyzer);
    record->clear();
    record->setPrimaryKey(1007);
    record->setSearchableAttributeValue(0, "Led Zepplelin");
    record->setSearchableAttributeValue(1, "Stairway to Heaven pink floyd");
    record->setRecordBoost(80);
    record->setInMemoryData(testRecord.c_str(), testRecord.length());
    index->addRecord(record, analyzer);
    record->clear();
    record->setPrimaryKey(1008);
    record->setSearchableAttributeValue(0, "Jimi Hendrix");
    record->setSearchableAttributeValue(1, "Little wing");
    record->setRecordBoost(90);
    record->setInMemoryData(testRecord.c_str(), testRecord.length());
    index->addRecord(record, analyzer);
    ///TODO: Assert that This record must not be added
    /// 1) Repeat of primary key
    /// 2) Check when adding junk data liek &*^#^%%
    /*record->clear();
     record->setPrimaryKey(1001);
     record->setAttributeValue(0, "jimi pink");
     record->setAttributeValue(1, "comes junk 2345 $%^^#");
     record->setBoost(100);
     index->addRecordBeforeCommit(record);*/
    index->commit();
    index->save();

    delete schema;
    delete record;
    delete analyzer;
    delete index;
}

bool checkResults(QueryResults *queryResults, set<unsigned> *resultSet) {
    for (unsigned resultCounter = 0;
            resultCounter < queryResults->getNumberOfResults();
            resultCounter++) {
        vector<string> matchingKeywords;
        vector<unsigned> editDistances;

        queryResults->getMatchingKeywords(resultCounter, matchingKeywords);
        queryResults->getEditDistances(resultCounter, editDistances);


        Logger::debug("Result-(%d) RecordId:%s\tScore:%s",
        		resultCounter,
        		(queryResults->getRecordId(resultCounter)).c_str(), queryResults->getResultScore(resultCounter).toString().c_str());
        Logger::debug("Matching Keywords:");

        unsigned counter = 0;
        for (vector<string>::iterator iter = matchingKeywords.begin();
                iter != matchingKeywords.end(); iter++, counter++) {
            Logger::debug("\t%s %d", (*iter).c_str(),
                    editDistances.at(counter));
        }

        if (resultSet->count(
                atoi(queryResults->getRecordId(resultCounter).c_str()))
                == false) {
            return false;
        }

    }
    return true;
}

void printResults(QueryResults *queryResults) {
    cout << "Results:" << queryResults->getNumberOfResults() << endl;
    for (unsigned resultCounter = 0;
            resultCounter < queryResults->getNumberOfResults();
            resultCounter++) {
        cout << "Result-(" << resultCounter << ") RecordId:"
                << queryResults->getRecordId(resultCounter) << endl;
        cout << "[" << queryResults->getInternalRecordId(resultCounter) << "]"
                << endl;
        cout << "[" << queryResults->getInMemoryRecordString_Safe(resultCounter)
                << "]" << endl;
    }
}

void Test_Complete_Exact(QueryEvaluator * queryEvaluator) {
    set<unsigned> resultSet0, resultSet1, resultSet2;
    resultSet0.insert(1007);
    resultSet0.insert(1006);
    resultSet0.insert(1003);
    //resultSet0.insert(4);

    resultSet1.insert(1007);
    resultSet1.insert(1003);
    //resultSet1.insert(4);

    resultSet2.insert(1003);
    //resultSet2.insert(3);
    //resultSet2.insert(4);

    int resultCount = 10;
    // create a query
    Query *query = new Query(srch2is::SearchTypeTopKQuery);
    string keywords[3] = { "pink", "floyd", "shine"};

    cout << "\n***COMPLETE EXACT***\nQuery:";
    TermType termType = TERM_TYPE_COMPLETE;
    cout << keywords[0] << "\n";
    Term *term0 = ExactTerm::create(keywords[0], termType, 1, 1);
    query->add(term0);
    QueryResults *queryResults = new QueryResults(new QueryResultFactory(),
            queryEvaluator, query);

    LogicalPlan * logicalPlan = prepareLogicalPlanForUnitTests(query , NULL, 0, 10, false, srch2::instantsearch::SearchTypeTopKQuery);
    resultCount = queryEvaluator->search(logicalPlan, queryResults);
    ASSERT(checkResults(queryResults, &resultSet0) == true);
    printResults(queryResults);

    cout << "\nAdding Term:";
    cout << keywords[1] << "\n";
    Term *term1 = ExactTerm::create(keywords[1], termType, 1, 1);
    query->add(term1);
    QueryResults *queryResults1 = new QueryResults(new QueryResultFactory(),
            queryEvaluator, query);
    logicalPlan = prepareLogicalPlanForUnitTests(query , NULL, 0, 10, false, srch2::instantsearch::SearchTypeTopKQuery);

    resultCount = queryEvaluator->search(logicalPlan, queryResults1);
    checkResults(queryResults1, &resultSet1);
    printResults(queryResults1);

    cout << "\nAdding Term:";
    cout << keywords[2] << "\n";
    Term *term2 = ExactTerm::create(keywords[2], termType, 1, 1);
    query->add(term2);
    QueryResults *queryResults2 = new QueryResults(new QueryResultFactory(),
    		queryEvaluator, query);
    logicalPlan = prepareLogicalPlanForUnitTests(query , NULL, 0, 10, false, srch2::instantsearch::SearchTypeTopKQuery);
    resultCount = queryEvaluator->search(logicalPlan, queryResults2);
    checkResults(queryResults2, &resultSet2);
    printResults(queryResults2);

    delete query;
    delete queryResults;
    delete queryResults1;
    delete queryResults2;
}

void Test_Prefix_Exact(QueryEvaluator * queryEvaluator) {
    set<unsigned> resultSet0, resultSet1, resultSet2;
    resultSet0.insert(1007);
    resultSet0.insert(1006);
    resultSet0.insert(1003);
    resultSet0.insert(1005);

    resultSet1.insert(1007);
    resultSet1.insert(1003);
    resultSet1.insert(1005);

    resultSet2.insert(1003);
    resultSet2.insert(1005);
    //resultSet2.insert(4);

    int resultCount = 10;
    // create a query
    Query *query = new Query(srch2is::SearchTypeTopKQuery);
    string keywords[3] = {
            "pin","floy","shi"
    };

    cout << "\n***PREFIX EXACT***\nQuery:";
    TermType termType = TERM_TYPE_PREFIX;
    cout << keywords[0] << "\n";
    Term *term0 = ExactTerm::create(keywords[0], termType, 1, 1);
    query->add(term0);
    QueryResults *queryResults = new QueryResults(new QueryResultFactory(),
    		queryEvaluator, query);
    LogicalPlan * logicalPlan = prepareLogicalPlanForUnitTests(query , NULL, 0, 10, false, srch2::instantsearch::SearchTypeTopKQuery);
    resultCount = queryEvaluator->search(logicalPlan, queryResults);
    checkResults(queryResults, &resultSet0);

    cout << "\nAdding Term:";
    cout << keywords[1] << "\n";
    Term *term1 = ExactTerm::create(keywords[1], termType, 1, 1);
    query->add(term1);
    QueryResults *queryResults1 = new QueryResults(new QueryResultFactory(),
    		queryEvaluator, query);
    logicalPlan = prepareLogicalPlanForUnitTests(query , NULL, 0, 10, false, srch2::instantsearch::SearchTypeTopKQuery);
    resultCount = queryEvaluator->search(logicalPlan, queryResults1);
    checkResults(queryResults1, &resultSet1);

    cout << "\nAdding Term:";
    cout << keywords[2] << "\n";
    Term *term2 = ExactTerm::create(keywords[2], termType, 1, 1);
    query->add(term2);
    QueryResults *queryResults2 = new QueryResults(new QueryResultFactory(),
    		queryEvaluator, query);
    logicalPlan = prepareLogicalPlanForUnitTests(query , NULL, 0, 10, false, srch2::instantsearch::SearchTypeTopKQuery);
    resultCount = queryEvaluator->search(logicalPlan, queryResults2);
    checkResults(queryResults2, &resultSet2);
    //printResults(queryResults2);

    delete query;
    delete queryResults;
    delete queryResults1;
    delete queryResults2;
}

void Test_Complete_Fuzzy(QueryEvaluator * queryEvaluator) {
    set<unsigned> resultSet0, resultSet1, resultSet2;
    resultSet0.insert(1007);
    resultSet0.insert(1006);
    resultSet0.insert(1003);

    resultSet1.insert(1007);
    resultSet1.insert(1003);

    resultSet2.insert(1003);

    int resultCount = 10;
    // create a query
    Query *query = new Query(srch2is::SearchTypeTopKQuery);
    string keywords[3] = {
            "pgnk","flayd","sheine"
    };

    cout << "\n***COMPLETE FUZZY***\nQuery:";
    TermType type = TERM_TYPE_COMPLETE;
    cout << keywords[0] << "\n";
    Term *term0 = FuzzyTerm::create(keywords[0], type, 1, 1, 2);
    query->add(term0);
    QueryResults *queryResults = new QueryResults(new QueryResultFactory(),
    		queryEvaluator, query);
    LogicalPlan * logicalPlan = prepareLogicalPlanForUnitTests(query , NULL, 0, 10, false, srch2::instantsearch::SearchTypeTopKQuery);
    resultCount = queryEvaluator->search(logicalPlan, queryResults);
    checkResults(queryResults, &resultSet0);

    cout << "\nAdding Term:";
    cout << keywords[1] << "\n";
    Term *term1 = FuzzyTerm::create(keywords[1], type, 1, 1, 2);
    query->add(term1);
    QueryResults *queryResults1 = new QueryResults(new QueryResultFactory(),
    		queryEvaluator, query);
    logicalPlan = prepareLogicalPlanForUnitTests(query , NULL, 0, 10, false, srch2::instantsearch::SearchTypeTopKQuery);
    resultCount = queryEvaluator->search(logicalPlan, queryResults1);
    checkResults(queryResults1, &resultSet1);

    cout << "\nAdding Term:";
    cout << keywords[2] << "\n";
    Term *term2 = FuzzyTerm::create(keywords[2], type, 1, 1, 2);
    query->add(term2);
    QueryResults *queryResults2 = new QueryResults(new QueryResultFactory(),
    		queryEvaluator, query);
    logicalPlan = prepareLogicalPlanForUnitTests(query , NULL, 0, 10, false, srch2::instantsearch::SearchTypeTopKQuery);
    resultCount = queryEvaluator->search(logicalPlan, queryResults2);
    checkResults(queryResults2, &resultSet2);

    delete query;
    delete queryResults;
    delete queryResults1;
    delete queryResults2;
}

void Test_Prefix_Fuzzy(QueryEvaluator * queryEvaluator) {
    set<unsigned> resultSet0, resultSet1, resultSet2;
    resultSet0.insert(1007);
    resultSet0.insert(1006);
    resultSet0.insert(1003);
    resultSet0.insert(1005);

    resultSet1.insert(1007);
    resultSet1.insert(1003);
    resultSet1.insert(1005);

    resultSet2.insert(1003);
    resultSet2.insert(1005);
    //resultSet2.insert(4);

    int resultCount = 10;
    // create a query
    Query *query = new Query(srch2is::SearchTypeTopKQuery);
    string keywords[3] = {
            "pionn","fllio","shiii"
    };

    cout << "\n***PREFIX FUZZY***\nQuery:";
    TermType type = TERM_TYPE_PREFIX;
    cout << keywords[0] << "\n";
    Term *term0 = FuzzyTerm::create(keywords[0], type, 1, 1, 2);
    query->add(term0);
    QueryResults *queryResults = new QueryResults(new QueryResultFactory(),
    		queryEvaluator, query);
    LogicalPlan * logicalPlan = prepareLogicalPlanForUnitTests(query , NULL, 0, 10, false, srch2::instantsearch::SearchTypeTopKQuery);
    resultCount = queryEvaluator->search(logicalPlan, queryResults);
    checkResults(queryResults, &resultSet0);

    cout << "\nAdding Term:";
    cout << keywords[1] << "\n";
    Term *term1 = FuzzyTerm::create(keywords[1], type, 1, 1, 2);
    query->add(term1);
    QueryResults *queryResults1 = new QueryResults(new QueryResultFactory(),
    		queryEvaluator, query);
    logicalPlan = prepareLogicalPlanForUnitTests(query , NULL, 0, 10, false, srch2::instantsearch::SearchTypeTopKQuery);
    resultCount = queryEvaluator->search(logicalPlan, queryResults1);
    checkResults(queryResults1, &resultSet1);

    cout << "\nAdding Term:";
    cout << keywords[2] << "\n";
    Term *term2 = FuzzyTerm::create(keywords[2], type, 1, 1, 2);
    query->add(term2);
    QueryResults *queryResults2 = new QueryResults(new QueryResultFactory(),
    		queryEvaluator, query);
    logicalPlan = prepareLogicalPlanForUnitTests(query , NULL, 0, 10, false, srch2::instantsearch::SearchTypeTopKQuery);
    resultCount = queryEvaluator->search(logicalPlan, queryResults2);
    checkResults(queryResults2, &resultSet2);

    delete query;
    delete queryResults;
    delete queryResults1;
    delete queryResults2;

}
void Searcher_Tests() {
    addRecords();

    unsigned mergeEveryNSeconds = 3;
    unsigned mergeEveryMWrites = 5;
    unsigned updateHistogramEveryPMerges = 1;
    unsigned updateHistogramEveryQWrites = 5;
    srch2is::IndexMetaData *indexMetaData = new srch2is::IndexMetaData(
            new CacheManager(), mergeEveryNSeconds, mergeEveryMWrites,
            updateHistogramEveryPMerges, updateHistogramEveryQWrites,
            INDEX_DIR);

    Indexer* indexer = Indexer::load(indexMetaData);

    QueryEvaluatorRuntimeParametersContainer runTimeParameters(10000, 500, 500);
    QueryEvaluator * queryEvaluator =
    		new srch2is::QueryEvaluator(indexer , &runTimeParameters );
//    QueryEvaluatorInternal * queryEvaluatorInternal = queryEvaluator->impl;

    Test_Complete_Exact(queryEvaluator);
    std::cout << "test1" << std::endl;

    Test_Prefix_Exact(queryEvaluator);
    std::cout << "test2" << std::endl;

    Test_Complete_Fuzzy(queryEvaluator);
    std::cout << "test3" << std::endl;

    Test_Prefix_Fuzzy(queryEvaluator);
    std::cout << "test4" << std::endl;

    delete queryEvaluator;
    delete indexer;
}

int main(int argc, char *argv[]) {
    bool verbose = false;
    if (argc > 1 && strcmp(argv[1], "--verbose") == 0) {
        verbose = true;
    }

    ActiveNodeSet_test();

    Searcher_Tests();

    cout << "IndexSearcherInternal Unit Tests: Passed\n";

    return 0;
}
