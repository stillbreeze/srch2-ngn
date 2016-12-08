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
#include "analyzer/AnalyzerInternal.h"
#include "util/Logger.h"
#include <instantsearch/Term.h>
#include <instantsearch/Schema.h>
#include <instantsearch/Record.h>
#include <instantsearch/QueryResults.h>
#include <instantsearch/Indexer.h>
#include <instantsearch/ResultsPostProcessor.h>
#include <wrapper/SortFilterEvaluator.h>
#include "../../core/unit/UnitTestHelper.h"
//
#include <iostream>
#include <functional>
#include <vector>
#include <string>
#include <algorithm>
#include "util/RecordSerializer.h"
#include "util/RecordSerializerUtil.h"
using namespace srch2::util;
using namespace std;
namespace srch2is = srch2::instantsearch;
using namespace srch2is;

typedef Trie Trie_Internal;

const char* INDEX_DIR = ".";

using srch2::util::Logger;

QueryResults * applyFilter(QueryResults * initialQueryResults,
        QueryEvaluator * queryEvaluator, Query * query,
        ResultsPostProcessorPlan * plan) {

//    ResultsPostProcessor postProcessor(indexSearcher);

    QueryResults * finalQueryResults = new QueryResults(
            new QueryResultFactory(), queryEvaluator, query);

    plan->beginIteration();

    // short circuit in case the plan doesn't have any filters in it.
    // if no plan is set in Query or there is no filter in it,
    // then there is no post processing so just mirror the results
    if (plan == NULL) {
        finalQueryResults->copyForPostProcessing(initialQueryResults);
        return finalQueryResults;
    }

    plan->beginIteration();
    if (!plan->hasMoreFilters()) {
        finalQueryResults->copyForPostProcessing(initialQueryResults);
        plan->closeIteration();
        return finalQueryResults;
    }

    // iterating on filters and applying them on list of results
    while (plan->hasMoreFilters()) {
        ResultsPostProcessorFilter * filter = plan->nextFilter();

        // clear the output to be ready to accept the result of the filter
        finalQueryResults->clear();
        // apply the filter on the input and put the results in output
        filter->doFilter(queryEvaluator, query, initialQueryResults,
                finalQueryResults);
        // if there is going to be other filters, chain the output to the input
        if (plan->hasMoreFilters()) {
            initialQueryResults->copyForPostProcessing(finalQueryResults);
        }

    }
    plan->closeIteration();

    return finalQueryResults;
}

void addRecords() {
    /// Create Schema
    Schema *schema = Schema::create(srch2::instantsearch::DefaultIndex);
    schema->setPrimaryKey("article_id"); // integer, not searchable
    schema->setSearchableAttribute("article_id"); // convert id to searchable text
    schema->setSearchableAttribute("article_authors", 2); // searchable text
    schema->setSearchableAttribute("article_title", 7); // searchable text
    schema->setRefiningAttribute("citation", ATTRIBUTE_TYPE_INT, "0");
    schema->setRefiningAttribute("price", ATTRIBUTE_TYPE_FLOAT, "1.25");
    schema->setRefiningAttribute("class", ATTRIBUTE_TYPE_TEXT, "Z");

    Record *record = new Record(schema);
    Analyzer *analyzer = new Analyzer(NULL, NULL, NULL, NULL, "");

    Schema * storedSchema = Schema::create();
    RecordSerializerUtil::populateStoredSchema(storedSchema, schema);

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
            "Tom Smith and Jack Lennon zzzzzz");
    record->setSearchableAttributeValue("article_title",
            "come Yesterday Once More");
    record->setRefiningAttributeValue("citation", "1");
    record->setRefiningAttributeValue("price", "10.34");
    record->setRefiningAttributeValue("class", "A");
    record->setRecordBoost(10);
    {
    	RecordSerializer recSerializer(*storedSchema);
    	recSerializer.addRefiningAttribute("citation", 1);
    	recSerializer.addRefiningAttribute("price", (float)10.34);
    	recSerializer.addSearchableAttribute("class",(string)"A");
    	RecordSerializerBuffer compactBuffer = recSerializer.serialize();
    	record->setInMemoryData(compactBuffer.start, compactBuffer.length);
    }
    index->addRecord(record, analyzer);

    record->clear();
    record->setPrimaryKey(1002);
    record->setSearchableAttributeValue(0, "George Harris zzzzzz");
    record->setSearchableAttributeValue(1, "Here comes the sun");
    record->setRefiningAttributeValue("citation", "2");
    record->setRefiningAttributeValue("price", "9.34");
    record->setRefiningAttributeValue("class", "A");
    record->setRecordBoost(20);
    {
        RecordSerializer recSerializer(*storedSchema);
        recSerializer.addRefiningAttribute("citation", 2);
        recSerializer.addRefiningAttribute("price", (float)9.34);
        recSerializer.addSearchableAttribute("class", (string)"A");
        RecordSerializerBuffer compactBuffer = recSerializer.serialize();
        record->setInMemoryData(compactBuffer.start, compactBuffer.length);
    }

    index->addRecord(record, analyzer);

    record->clear();
    record->setPrimaryKey(1003);
    record->setSearchableAttributeValue(0, "Pink Floyd zzzzzz");
    record->setSearchableAttributeValue(1, "Shine on you crazy diamond");
    record->setRefiningAttributeValue("citation", "3");
    record->setRefiningAttributeValue("price", "8.34");
    record->setRefiningAttributeValue("class", "B");
    record->setRecordBoost(30);
    {
        RecordSerializer recSerializer(*storedSchema);
        recSerializer.addRefiningAttribute("citation", 3);
        recSerializer.addRefiningAttribute("price", (float)8.34);
        recSerializer.addSearchableAttribute("class", (string)"B");
        RecordSerializerBuffer compactBuffer = recSerializer.serialize();
        record->setInMemoryData(compactBuffer.start, compactBuffer.length);
    }


    index->addRecord(record,analyzer);

    record->clear();
    record->setPrimaryKey(1004);
    record->setSearchableAttributeValue(0, "Uriah Hepp zzzzzz");
    record->setSearchableAttributeValue(1, "Come Shine away Melinda ");
    record->setRefiningAttributeValue("citation", "4");
    record->setRefiningAttributeValue("price", "7.34");
    record->setRefiningAttributeValue("class", "B");
    record->setRecordBoost(40);
    {
        RecordSerializer recSerializer(*storedSchema);
        recSerializer.addRefiningAttribute("citation", 4);
        recSerializer.addRefiningAttribute("price", (float)7.34);
        recSerializer.addSearchableAttribute("class", (string)"B");
        RecordSerializerBuffer compactBuffer = recSerializer.serialize();
        record->setInMemoryData(compactBuffer.start, compactBuffer.length);
    }


    index->addRecord(record, analyzer);

    record->clear();
    record->setPrimaryKey(1005);
    record->setSearchableAttributeValue(0, "Pinksyponzi Floydsyponzi zzzzzz");
    record->setSearchableAttributeValue(1,
            "Shinesyponzi on Wish you were here");
    record->setRefiningAttributeValue("citation", "5");
    record->setRefiningAttributeValue("price", "6.34");
    record->setRefiningAttributeValue("class", "C");
    record->setRecordBoost(50);
    record->setRecordBoost(40);
    {
        RecordSerializer recSerializer(*storedSchema);
        recSerializer.addRefiningAttribute("citation", 5);
        recSerializer.addRefiningAttribute("price", (float)6.34);
        recSerializer.addSearchableAttribute("class", (string)"C");
        RecordSerializerBuffer compactBuffer = recSerializer.serialize();
        record->setInMemoryData(compactBuffer.start, compactBuffer.length);
    }

    index->addRecord(record,analyzer);

    record->clear();
    record->setPrimaryKey(1006);
    record->setSearchableAttributeValue(0, "U2 2345 Pink zzzzzz");
    record->setSearchableAttributeValue(1, "with or without you");
    record->setRefiningAttributeValue("citation", "6");
    record->setRefiningAttributeValue("price", "5.34");
    record->setRefiningAttributeValue("class", "C");
    record->setRecordBoost(60);
    record->setRecordBoost(40);
    {
        RecordSerializer recSerializer(*storedSchema);
        recSerializer.addRefiningAttribute("citation", 6);
        recSerializer.addRefiningAttribute("price", (float)5.34);
        recSerializer.addSearchableAttribute("class", (string)"C");
        RecordSerializerBuffer compactBuffer = recSerializer.serialize();
        record->setInMemoryData(compactBuffer.start, compactBuffer.length);
    }

    index->addRecord(record, analyzer);

    record->clear();
    record->setPrimaryKey(1007);
    record->setSearchableAttributeValue(0, "Led Zepplelin zzzzzz");
    record->setSearchableAttributeValue(1, "Stairway to Heaven pink floyd");
    record->setRefiningAttributeValue("citation", "7");
    record->setRefiningAttributeValue("price", "4.34");
    record->setRefiningAttributeValue("class", "D");
    record->setRecordBoost(80);
    record->setRecordBoost(40);
    {
        RecordSerializer recSerializer(*storedSchema);
        recSerializer.addRefiningAttribute("citation", 7);
        recSerializer.addRefiningAttribute("price", (float)4.34);
        recSerializer.addSearchableAttribute("class", (string)"D");
        RecordSerializerBuffer compactBuffer = recSerializer.serialize();
        record->setInMemoryData(compactBuffer.start, compactBuffer.length);
    }

    index->addRecord(record, analyzer);

    record->clear();
    record->setPrimaryKey(1008);
    record->setSearchableAttributeValue(0, "Jimi Hendrix zzzzzz");
    record->setSearchableAttributeValue(1, "Little wing");
    record->setRefiningAttributeValue("citation", "8");
    record->setRefiningAttributeValue("price", "3.34");
    record->setRefiningAttributeValue("class", "E");
    record->setRecordBoost(90);
    record->setRecordBoost(40);
    {
        RecordSerializer recSerializer(*storedSchema);
        recSerializer.addRefiningAttribute("citation", 8);
        recSerializer.addRefiningAttribute("price", (float)3.34);
        recSerializer.addSearchableAttribute("class",(string) "E");
        RecordSerializerBuffer compactBuffer = recSerializer.serialize();
        record->setInMemoryData(compactBuffer.start, compactBuffer.length);
    }

    index->addRecord(record, analyzer);

    index->commit();
    index->save();

    delete schema;
    delete record;
    delete analyzer;
    delete index;
}

bool checkResults(QueryResults *queryResults, vector<unsigned> *resultSet)

{
    if (queryResults->getNumberOfResults() != resultSet->size())
        return false;
    for (unsigned resultCounter = 0;
            resultCounter < queryResults->getNumberOfResults();
            resultCounter++) {
        vector<string> matchingKeywords;
        vector<unsigned> editDistances;

        queryResults->getMatchingKeywords(resultCounter, matchingKeywords);
        queryResults->getEditDistances(resultCounter, editDistances);

        if (resultSet->at(resultCounter)
                != atoi(queryResults->getRecordId(resultCounter).c_str())) {

            Logger::info(
                    "\n=====================================================");
            for (unsigned resultCounter = 0;
                    resultCounter < queryResults->getNumberOfResults();
                    resultCounter++) {
                Logger::info("\nResult-(%d) RecordId: %s", resultCounter,
                        queryResults->getRecordId(resultCounter).c_str());
            }
            return false;
        }

    }
    return true;
}
bool checkFacetResultsHelper(
        const std::map<std::string, std::pair<FacetType ,  std::vector<std::pair<std::string, float> > > > * facetResults,
        const std::map<std::string, std::pair<FacetType ,  std::vector<std::pair<std::string, float> > > > * facetResultsForTest) {

    if (facetResultsForTest->size() != facetResults->size()) {

        return false;
    }
    for (std::map<std::string, std::pair<FacetType ,  std::vector<std::pair<std::string, float> > > >::const_iterator attrIter =
            facetResults->begin(); attrIter != facetResults->end();
            ++attrIter) {
        const std::vector<std::pair<std::string, float> > & toTest =
                facetResultsForTest->at(attrIter->first).second;
        const std::vector<std::pair<std::string, float> > & correct =
                facetResults->at(attrIter->first).second;

        bool flag = true;
        for (std::vector<std::pair<std::string, float> >::const_iterator correctPair =
                correct.begin(); correctPair != correct.end(); ++correctPair) {
            for (std::vector<std::pair<std::string, float> >::const_iterator toTestPair =
                    correct.begin(); toTestPair != correct.end();
                    ++toTestPair) {
                if (correctPair->first.compare(toTestPair->first) == 0) {
                    if (correctPair->second != toTestPair->second) {
                        flag = false;
                        break;
                    } else {
                        break;
                    }

                }
            }
            if (flag == false)
                break;
        }

        if (flag)
            continue;

        return false;

    }
    return true;
}

void printFacetResults(
        const std::map<std::string, std::pair<FacetType , std::vector<std::pair<std::string, float> > > > * facetResults) {
    Logger::info("Facet results : \n");
    for (std::map<std::string, std::pair<FacetType , std::vector<std::pair<std::string, float> > > >::const_iterator attrIter =
            facetResults->begin(); attrIter != facetResults->end();
            ++attrIter) {
        Logger::info("Attribute : %s\n", attrIter->first.c_str());
        for (std::vector<std::pair<std::string, float> >::const_iterator aggregationResult =
                attrIter->second.second.begin();
                aggregationResult != attrIter->second.second.end();
                ++aggregationResult) {
            Logger::info("\t%s : %.3f \n", aggregationResult->first.c_str(),
                    aggregationResult->second);
        }
        Logger::info("\n ======================================= \n");
    }
}

bool checkFacetedFilterResults(QueryResults * queryResults,
        const std::map<std::string, std::pair<FacetType ,  std::vector<std::pair<std::string, float> > > > * facetResults) {
    const std::map<std::string, std::pair<FacetType ,  std::vector<std::pair<std::string, float> > > > * facetResultsForTest =
            queryResults->getFacetResults();

    bool match = checkFacetResultsHelper(facetResults, facetResultsForTest);

    if (!match) {
        Logger::info("Facet results : \n");
        for (std::map<std::string, std::pair<FacetType ,  std::vector<std::pair<std::string, float> > > >::const_iterator attrIter =
                facetResultsForTest->begin();
                attrIter != facetResultsForTest->end(); ++attrIter) {
            Logger::info("Attribute : %s\n", attrIter->first.c_str());
            for (std::vector<std::pair<std::string, float> >::const_iterator aggregationResult =
                    attrIter->second.second.begin();
                    aggregationResult != attrIter->second.second.end();
                    ++aggregationResult) {
                Logger::info("\t%s : %.3f \n", aggregationResult->first.c_str(),
                        aggregationResult->second);
            }
            Logger::info("\n ======================================= \n");
        }
        Logger::info("Correct Facet results : \n");
        for (std::map<std::string, std::pair<FacetType ,  std::vector<std::pair<std::string, float> > > >::const_iterator attrIter =
                facetResults->begin(); attrIter != facetResults->end();
                ++attrIter) {
            Logger::info("Attribute : %s\n", attrIter->first.c_str());
            for (std::vector<std::pair<std::string, float> >::const_iterator aggregationResult =
                    attrIter->second.second.begin();
                    aggregationResult != attrIter->second.second.end();
                    ++aggregationResult) {
                Logger::info("\t%s : %.3f \n", aggregationResult->first.c_str(),
                        aggregationResult->second);
            }
            Logger::info("\n ======================================= \n");
        }
        return match;
    }

    match = checkFacetResultsHelper(facetResultsForTest, facetResults);

    if (!match) {
        Logger::info("Facet results : \n");
        for (std::map<std::string, std::pair<FacetType ,  std::vector<std::pair<std::string, float> > > >::const_iterator attrIter =
                facetResultsForTest->begin();
                attrIter != facetResultsForTest->end(); ++attrIter) {
            Logger::info("Attribute : %s\n", attrIter->first.c_str());
            for (std::vector<std::pair<std::string, float> >::const_iterator aggregationResult =
                    attrIter->second.second.begin();
                    aggregationResult != attrIter->second.second.end();
                    ++aggregationResult) {
                Logger::info("\t%s : %.3f \n", aggregationResult->first.c_str(),
                        aggregationResult->second);
            }
            Logger::info("\n ======================================= \n");
        }
        Logger::info("Correct Facet results : \n");
        for (std::map<std::string, std::pair<FacetType ,  std::vector<std::pair<std::string, float> > > >::const_iterator attrIter =
                facetResults->begin(); attrIter != facetResults->end();
                ++attrIter) {
            Logger::info("Attribute : %s\n", attrIter->first.c_str());
            for (std::vector<std::pair<std::string, float> >::const_iterator aggregationResult =
                    attrIter->second.second.begin();
                    aggregationResult != attrIter->second.second.end();
                    ++aggregationResult) {
                Logger::info("\t%s : %.3f \n", aggregationResult->first.c_str(),
                        aggregationResult->second);
            }
            Logger::info("\n ======================================= \n");
        }
        return match;
    }

    return match;

}

void Test_Sort_Filter(QueryEvaluator *queryEvaluator) {

    std::vector<unsigned> resultSet0, resultSet01, resultSet02, resultSet03,
            resultSet1, resultSet2, resultSetEmpty;

    // query = "pionn", sort: citation
    resultSet0.push_back(1003);
    resultSet0.push_back(1005);
    resultSet0.push_back(1006);
    resultSet0.push_back(1007);

    // query = "pionn", sort: price
    resultSet01.push_back(1007);
    resultSet01.push_back(1006);
    resultSet01.push_back(1005);
    resultSet01.push_back(1003);

    // query = "pionn", sort: class
    resultSet02.push_back(1003);
    resultSet02.push_back(1005);
    resultSet02.push_back(1006);
    resultSet02.push_back(1007);

    // query = "pionn", sort: class,citation
    resultSet03.push_back(1003);
    resultSet03.push_back(1005);
    resultSet03.push_back(1006);
    resultSet03.push_back(1007);

    int resultCount = 10;
    // create a query
    Query *query = new Query(srch2is::SearchTypeTopKQuery);
    string keywords[3] = { "pionn", "fllio", "shiii" };

    Logger::info("***PREFIX FUZZY***\nQuery:");
    TermType type = srch2::instantsearch::TERM_TYPE_PREFIX;
    Logger::info(keywords[0].c_str());

    Term *term0 = FuzzyTerm::create(keywords[0], type, 1, 1, 2);
    query->add(term0);

    ResultsPostProcessorPlan * plan = NULL;
    plan = new ResultsPostProcessorPlan();
    srch2::httpwrapper::SortFilterEvaluator * eval =
            new srch2::httpwrapper::SortFilterEvaluator();
    eval->order = srch2::instantsearch::SortOrderAscending;
    eval->field.push_back("citation");

    QueryResults *queryResults = new QueryResults(new QueryResultFactory(),
    		queryEvaluator, query);
    LogicalPlan * logicalPlan = prepareLogicalPlanForUnitTests(query , NULL, 0, resultCount, false, srch2::instantsearch::SearchTypeTopKQuery);
    logicalPlan->setPostProcessingInfo(new ResultsPostProcessingInfo());
    logicalPlan->getPostProcessingInfo()->setSortEvaluator(eval);
    queryEvaluator->search(logicalPlan , queryResults);

    bool valid = checkResults(queryResults, &resultSet0);
    ASSERT(valid);

    ////////////////////////////////////////////////////

    plan = NULL;
    plan = new ResultsPostProcessorPlan();
    eval = new srch2::httpwrapper::SortFilterEvaluator();
    eval->order = srch2::instantsearch::SortOrderAscending;
    eval->field.push_back("price");

    queryResults = new QueryResults(new QueryResultFactory(),
            queryEvaluator, query);
    logicalPlan = prepareLogicalPlanForUnitTests(query , NULL, 0, resultCount, false, srch2::instantsearch::SearchTypeTopKQuery);
    logicalPlan->setPostProcessingInfo(new ResultsPostProcessingInfo());
    logicalPlan->getPostProcessingInfo()->setSortEvaluator(eval);
    queryEvaluator->search(logicalPlan , queryResults);

    valid = checkResults(queryResults, &resultSet01);
    ASSERT(valid);

    ////////////////////////////////////////////////////////

    plan = NULL;
    plan = new ResultsPostProcessorPlan();
    eval = new srch2::httpwrapper::SortFilterEvaluator();
    eval->order = srch2::instantsearch::SortOrderAscending;
    eval->field.push_back("class");

    queryResults = new QueryResults(new QueryResultFactory(),
            queryEvaluator, query);
    logicalPlan = prepareLogicalPlanForUnitTests(query , NULL, 0, resultCount, false, srch2::instantsearch::SearchTypeTopKQuery);
    logicalPlan->setPostProcessingInfo(new ResultsPostProcessingInfo());
    logicalPlan->getPostProcessingInfo()->setSortEvaluator(eval);
        queryEvaluator->search(logicalPlan , queryResults);

    valid = checkResults(queryResults, &resultSet02);
    ASSERT(valid);

    ///////////////////////////////////////////////////////////

    plan = NULL;
    plan = new ResultsPostProcessorPlan();
    eval = new srch2::httpwrapper::SortFilterEvaluator();
    eval->order = srch2::instantsearch::SortOrderAscending;
    eval->field.push_back("class");
    eval->field.push_back("citation");

    queryResults = new QueryResults(new QueryResultFactory(),
            queryEvaluator, query);
    logicalPlan = prepareLogicalPlanForUnitTests(query , NULL, 0, resultCount, false, srch2::instantsearch::SearchTypeTopKQuery);
    logicalPlan->setPostProcessingInfo(new ResultsPostProcessingInfo());
    logicalPlan->getPostProcessingInfo()->setSortEvaluator(eval);
        queryEvaluator->search(logicalPlan , queryResults);

    valid = checkResults(queryResults, &resultSet03);
    ASSERT(valid);

    /////////////////////////////////////////////////////////////

    // test with empty results
    type = srch2is::TERM_TYPE_PREFIX;

    Logger::info("keywordwhichisnotinresults");

    Term *term1 = FuzzyTerm::create("keywordwhichisnotinresults", type, 1, 1,
            2);
    query->add(term1);

    plan = NULL;
    plan = new ResultsPostProcessorPlan();
    eval = new srch2::httpwrapper::SortFilterEvaluator();
    eval->order = srch2::instantsearch::SortOrderAscending;
    eval->field.push_back("price");

    queryResults = new QueryResults(new QueryResultFactory(),
            queryEvaluator, query);
    logicalPlan = prepareLogicalPlanForUnitTests(query , NULL, 0, resultCount, false, srch2::instantsearch::SearchTypeTopKQuery);
    logicalPlan->setPostProcessingInfo(new ResultsPostProcessingInfo());
    logicalPlan->getPostProcessingInfo()->setSortEvaluator(eval);
        queryEvaluator->search(logicalPlan , queryResults);

    valid = checkResults(queryResults, &resultSetEmpty);
    ASSERT(valid);

    delete query;
    delete queryResults;
}

void Test_FacetedSearch_Filter(QueryEvaluator *queryEvaluator) {

    std::vector<unsigned> resultSet0, resultSet01, resultSet02, resultSet03,
            resultSet1, resultSet2, resultSetEmpty;

    // query = "zzzzzzz", facet: class<A,B,C,D,E>
    resultSet0.push_back(1001);
    resultSet0.push_back(1002);
    resultSet0.push_back(1003);
    resultSet0.push_back(1004);
    resultSet0.push_back(1005);
    resultSet0.push_back(1006);
    resultSet0.push_back(1007);
    resultSet0.push_back(1008);

    std::map<std::string, std::pair<FacetType , std::vector<std::pair<std::string, float> > > > facetResults; // class , simple
    std::vector<std::pair<std::string, float> > classFacetResults;
    classFacetResults.push_back(std::make_pair("A", 2));
    classFacetResults.push_back(std::make_pair("B", 2));
    classFacetResults.push_back(std::make_pair("C", 2));
    classFacetResults.push_back(std::make_pair("D", 1));
    classFacetResults.push_back(std::make_pair("E", 1));
    facetResults["class"] = std::make_pair(FacetTypeCategorical , classFacetResults);
    //////////////////////////////////

    std::map<std::string, std::pair<FacetType , std::vector<std::pair<std::string, float> > > > facetResults2; //class , simple & citation : 1,5 range
    std::vector<std::pair<std::string, float> > classFacetResults2;
    classFacetResults2.push_back(std::make_pair("A", 2));
    classFacetResults2.push_back(std::make_pair("B", 2));
    classFacetResults2.push_back(std::make_pair("C", 2));
    classFacetResults2.push_back(std::make_pair("D", 1));
    classFacetResults2.push_back(std::make_pair("E", 1));
    facetResults2["class"] = std::make_pair(FacetTypeCategorical , classFacetResults2);

    std::vector<std::pair<std::string, float> > citationFacetResults2;
    citationFacetResults2.push_back(std::make_pair("0", 0));
    citationFacetResults2.push_back(std::make_pair("1", 4));
    citationFacetResults2.push_back(std::make_pair("5", 4));
    facetResults2["citation"] = std::make_pair(FacetTypeRange , citationFacetResults2);
    //////////////////////////////////

    std::map<std::string, std::pair<FacetType , std::vector<std::pair<std::string, float> > > > facetResults3; //price : 5,7 , range & class , simple & citation : 1 , range
    std::vector<std::pair<std::string, float> > priceFacetResults3;
    priceFacetResults3.push_back(std::make_pair("0", 2)); // < 5 : 3.34,4.34
    priceFacetResults3.push_back(std::make_pair("5", 2)); // >=5 : 5.34,6.34
    priceFacetResults3.push_back(std::make_pair("7", 4)); // >=7 : 7.34,8.34,9.34,10.34
    facetResults3["price"] = std::make_pair(FacetTypeRange , priceFacetResults3);
    std::vector<std::pair<std::string, float> > classFacetResults3;
    classFacetResults3.push_back(std::make_pair("A", 2));
    classFacetResults3.push_back(std::make_pair("B", 2));
    classFacetResults3.push_back(std::make_pair("C", 2));
    classFacetResults3.push_back(std::make_pair("D", 1));
    classFacetResults3.push_back(std::make_pair("E", 1));
    facetResults3["class"] = std::make_pair(FacetTypeCategorical , classFacetResults3);
    std::vector<std::pair<std::string, float> > citationFacetResults3;
    citationFacetResults3.push_back(std::make_pair("0", 0));
    citationFacetResults3.push_back(std::make_pair("1", 8));
    facetResults3["citation"] = std::make_pair(FacetTypeRange , citationFacetResults3);
    //////////////////////////////////

    int resultCount = 10;
    // create a query
    Query *query = new Query(srch2is::SearchTypeTopKQuery);
    string keywords[1] = { "zzzzzzz" };

    Logger::info("\n***PREFIX FUZZY***\nQuery:");

    // query = "zzzzzzz",
    TermType type = TERM_TYPE_PREFIX;
    Logger::info(keywords[0].c_str());
    Term *term0 = FuzzyTerm::create(keywords[0], type, 1, 1, 2);
    query->add(term0);

    // facet: // class , simple
    ResultsPostProcessorPlan * plan = NULL;
    plan = new ResultsPostProcessorPlan();

    FacetQueryContainer fqc;

    fqc.types.push_back(srch2::instantsearch::FacetTypeCategorical);
    fqc.fields.push_back("class");
    fqc.rangeStarts.push_back("");
    fqc.rangeEnds.push_back("");
    fqc.rangeGaps.push_back("");

    query->setPostProcessingPlan(plan);
    QueryResultFactory * factory = new QueryResultFactory();
    QueryResults *queryResults = new QueryResults(factory,
            queryEvaluator, query);
    LogicalPlan * logicalPlan = prepareLogicalPlanForUnitTests(query , NULL, 0, resultCount, false, srch2::instantsearch::SearchTypeTopKQuery);
    logicalPlan->setPostProcessingInfo(new ResultsPostProcessingInfo());
    logicalPlan->getPostProcessingInfo()->setFacetInfo(&fqc);
        queryEvaluator->search(logicalPlan , queryResults);

    bool valid = checkFacetedFilterResults(queryResults,
            &facetResults);
    printFacetResults(queryResults->getFacetResults());
    ASSERT(valid);

//    ASSERT(checkResults(queryResultsAfterFilter, &resultSet0));
    delete plan;
    delete queryResults;
    delete factory;
    ////////////////////////////////////////////////////
    // facet: //class , simple & citation : 1,5 range

    plan = NULL;
    plan = new ResultsPostProcessorPlan();

    fqc.types.clear();
    fqc.fields.clear();
    fqc.rangeStarts.clear();
    fqc.rangeEnds.clear();
    fqc.rangeGaps.clear();

    // class
    fqc.types.push_back(srch2::instantsearch::FacetTypeCategorical);
    fqc.fields.push_back("class");
    fqc.rangeStarts.push_back("");
    fqc.rangeEnds.push_back("");
    fqc.rangeGaps.push_back("");
    // citation
    fqc.types.push_back(srch2::instantsearch::FacetTypeRange);
    fqc.fields.push_back("citation");
    fqc.rangeStarts.push_back("1");
    fqc.rangeEnds.push_back("5");
    fqc.rangeGaps.push_back("4");

    query->setPostProcessingPlan(plan);
    factory = new QueryResultFactory();
    queryResults = new QueryResults(factory, queryEvaluator, query);
    logicalPlan = prepareLogicalPlanForUnitTests(query , NULL, 0, resultCount, false, srch2::instantsearch::SearchTypeTopKQuery);
    logicalPlan->setPostProcessingInfo(new ResultsPostProcessingInfo());
    logicalPlan->getPostProcessingInfo()->setFacetInfo(&fqc);
        queryEvaluator->search(logicalPlan , queryResults);

    valid = checkFacetedFilterResults(queryResults, &facetResults2);
    printFacetResults(queryResults->getFacetResults());
    ASSERT(valid);
    delete plan;
    delete queryResults;
    delete factory;

    //////////////////////////////////
    ////price : 5,7 , range & class , simple & citation : 1 , range
    plan = NULL;
    plan = new ResultsPostProcessorPlan();

    fqc.types.clear();
    fqc.fields.clear();
    fqc.rangeStarts.clear();
    fqc.rangeEnds.clear();
    fqc.rangeGaps.clear();


    // price
    fqc.types.push_back(srch2::instantsearch::FacetTypeRange);
    fqc.fields.push_back("price");
    fqc.rangeStarts.push_back("5");
    fqc.rangeEnds.push_back("7");
    fqc.rangeGaps.push_back("2");
    // class
    fqc.types.push_back(srch2::instantsearch::FacetTypeCategorical);
    fqc.fields.push_back("class");
    fqc.rangeStarts.push_back("");
    fqc.rangeEnds.push_back("");
    fqc.rangeGaps.push_back("");
    // citation
    fqc.types.push_back(srch2::instantsearch::FacetTypeRange);
    fqc.fields.push_back("citation");
    fqc.rangeStarts.push_back("1");
    fqc.rangeEnds.push_back("1");
    fqc.rangeGaps.push_back("0");

    query->setPostProcessingPlan(plan);
    factory = new QueryResultFactory();
    queryResults = new QueryResults(factory, queryEvaluator, query);
    logicalPlan = prepareLogicalPlanForUnitTests(query , NULL, 0, resultCount, false, srch2::instantsearch::SearchTypeTopKQuery);
    logicalPlan->setPostProcessingInfo(new ResultsPostProcessingInfo());
        logicalPlan->getPostProcessingInfo()->setFacetInfo(&fqc);
        queryEvaluator->search(logicalPlan , queryResults);

    ASSERT(checkFacetedFilterResults(queryResults, &facetResults3));
    delete plan;
    delete queryResults;
    delete factory;

}

void Searcher_Tests() {
    addRecords();

    unsigned mergeEveryNSeconds = 3;
    unsigned mergeEveryMWrites = 5;
    unsigned updateHistogramEveryPMerges = 1;
    unsigned updateHistogramEveryQWrites = 5;
    srch2is::IndexMetaData *indexMetaData = new srch2is::IndexMetaData(
            new CacheManager(), mergeEveryNSeconds, mergeEveryMWrites,
            updateHistogramEveryPMerges , updateHistogramEveryQWrites,
            INDEX_DIR);

    Indexer* indexer = Indexer::load(indexMetaData);

    QueryEvaluatorRuntimeParametersContainer runtimeParameters;
    QueryEvaluator * queryEvaluator = new QueryEvaluator(indexer, &runtimeParameters);
    Logger::info("Test 1");
    Test_Sort_Filter(queryEvaluator);
    Logger::info("Test 2");
    Test_FacetedSearch_Filter(queryEvaluator);

    delete indexer;
    delete queryEvaluator;
}
int main(int argc, char *argv[]) {
    bool verbose = false;
    if (argc > 1 && strcmp(argv[1], "--verbose") == 0) {
        verbose = true;
    }

    Searcher_Tests();

    Logger::info("Post Processing Filters Unit Tests: Passed\n");
    return 0;
}
//
