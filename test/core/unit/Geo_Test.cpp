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
 * QuadTree_Test.cpp
 *
 *  Created on: Jul 8, 2014
 */

/*
 *  for this test case set these variables in QuadTreeNode.h
 *
	const unsigned GEO_MAX_NUM_OF_ELEMENTS = 6;
	const double GEO_MIN_SEARCH_RANGE_SQUARE = (0.24 * 0.24);
	const double GEO_MIN_DISTANCE_SCORE = 0.05;
	const double GEO_MBR_LIMIT = (0.0005 * 0.0005);
	const unsigned GEO_CHILD_NUM_SQRT = 2;
	const unsigned GEO_CHILD_NUM = (GEO_CHILD_NUM_SQRT * GEO_CHILD_NUM_SQRT);

	and set these variables in QuadTree.h

	const double GEO_TOP_RIGHT_X = 200.0;
	const double GEO_TOP_RIGHT_Y = 200.0;
	const double GEO_BOTTOM_LEFT_X = -200.0;
	const double GEO_BOTTOM_LEFT_Y = -200.0;

 */

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <time.h>
#include <iomanip>
#include "geo/QuadTree.h"
#include "util/Assert.h"

#include <instantsearch/Query.h>
#include <instantsearch/Term.h>

#include <instantsearch/GlobalCache.h>

#include "../integration/MapSearchTestHelper.h"
#include "operation/TermVirtualList.h"
#include "operation/IndexerInternal.h"
#include "operation/IndexData.h"
#include "instantsearch/QueryEvaluator.h"
#include "operation/QueryEvaluatorInternal.h"

using namespace std;
using namespace srch2::instantsearch;

bool parseDouble(string &line, double &doubleNum)
{
	istringstream coordinatesStream(line);
	double num;
	vector<double> nums;
	while(coordinatesStream >> num)
	{
		nums.push_back(num);
	}
	if(nums.size() == 1)
	{
		doubleNum = nums[0];
	}
	else
	{
		return false;
	}
	return true;
}

bool parseUnsigned(string &line, unsigned &pk)
{
	istringstream coordinatesStream(line);
	unsigned num;
	vector<unsigned> nums;
	while(coordinatesStream >> num)
	{
		nums.push_back(num);
	}
	if(nums.size() == 1)
	{
		pk = nums[0];
	}
	else
	{
		return false;
	}
	return true;
}

bool parseMultiUnsigned(string &line, unsigned numOfResults, vector<unsigned> &expectedResults)
{
	istringstream coordinatesStream(line);
	unsigned num;
	vector<unsigned> nums;
	while(coordinatesStream >> num)
	{
		nums.push_back(num);
	}
	if(nums.size() == numOfResults)
	{
		expectedResults = nums;
	}
	else
	{
		return false;
	}
	return true;
}

bool parsePoint(string &line, Point &pt)
{
	istringstream coordinatesStream(line);
	double coord;
	vector<double> coords;
	while(coordinatesStream >> coord)
	{
		coords.push_back(coord);
	}
	if(coords.size() == 2)
	{
		pt.x = coords[0];
		pt.y = coords[1];
	}
	else
	{
		return false;
	}
	return true;
}

void verifyResults(vector<vector<GeoElement*>*> & results,vector<vector<unsigned>*> & expectedResults){
	ASSERT(results.size()==expectedResults.size());
	vector<GeoElement*> tmpRes;
	vector<unsigned> tmpExp;

	list<unsigned> resList;
	list<unsigned> expList;

	for(int i = 0;i<results.size();i++){
		tmpRes = *results[i];
		tmpExp =*expectedResults[i];
		for(int j = 0;j<tmpRes.size();j++){
			resList.push_back(tmpRes[j]->forwardListID);
		}
		for(int j = 0;j<tmpExp.size();j++){
			expList.push_back(tmpExp[j]);
		}
	}
	ASSERT(resList.size()==expList.size());

	resList.sort();
	expList.sort();

	list<unsigned>::const_iterator iterator1 = resList.begin();
	list<unsigned>::const_iterator iterator2 = expList.begin();
	while(iterator1 != resList.end()) {
		ASSERT(*iterator1 == *iterator2);
		iterator1++;
		iterator2++;
	}
}



void printResults(vector<vector<GeoElement*>*> & results){
	vector<GeoElement*> tmpRes;
	list<unsigned> resList;

	for(int i = 0;i<results.size();i++){
		tmpRes = *results[i];
		for(int j = 0;j<tmpRes.size();j++){
			resList.push_back(tmpRes[j]->forwardListID);
		}
	}
	resList.sort();

	list<unsigned>::const_iterator iterator1 = resList.begin();
	while(iterator1 != resList.end()) {
		cout << *iterator1 << "**";
		iterator1++;
	}
	cout << endl;
}

// get the externalId of a record from its internalid
unsigned getExternalId(QueryEvaluator * queryEvaluator, const unsigned internalRecordId){
	shared_ptr<vectorview<ForwardListPtr> > readView;
	queryEvaluator->impl->testOnly_getForwardIndex()->getForwardListDirectory_ReadView(readView);
	std::string externalRecordId;
	queryEvaluator->impl->testOnly_getForwardIndex()->getExternalRecordIdFromInternalRecordId(readView,internalRecordId,externalRecordId);
	return atoi(externalRecordId.c_str());
}

void addGeoRecord(Indexer *indexer, Schema *schema, Analyzer* analyzer, unsigned primaryKey, const string &firstAttribute, const string &secondAttribute, double pointX, double pointY)
{
	Point point;
	point.x = pointX;
	point.y = pointY;
	Record *record = new Record(schema);
	record->setPrimaryKey(primaryKey); // give a value for the primary key
	record->setSearchableAttributeValue(0, firstAttribute);
	record->setSearchableAttributeValue(1, secondAttribute);
	record->setLocationAttributeValue(point.x, point.y);

	indexer->addRecord(record, analyzer);

	delete record;
}


// Test the case where we only have one node(root) with a few records in the tree
void testSingleNodeQuadTree(string directoryName)
{
	// Create a schema
	Schema *schema = Schema::create(LocationIndex);
	schema->setPrimaryKey("list_id"); // integer, by default not searchable
	schema->setSearchableAttribute("title", 2); // searchable text
	schema->setSearchableAttribute("address", 7); // searchable text

    // Create an analyzer
    Analyzer *analyzer = new Analyzer(NULL, NULL, NULL, NULL, "");

    unsigned mergeEveryNSeconds = 3;
    unsigned mergeEveryMWrites = 5;
    unsigned updateHistogramEveryPMerges = 1;
    unsigned updateHistogramEveryQWrites = 5;
	CacheManager *cache = new CacheManager(134217728);
    IndexMetaData *indexMetaData = new IndexMetaData( cache,
    		mergeEveryNSeconds, mergeEveryMWrites,
    		updateHistogramEveryPMerges, updateHistogramEveryQWrites,
    		directoryName);

	Indexer *indexer = Indexer::create(indexMetaData, analyzer, schema);

	// Create five records of 3 attributes and add them to the index
	addGeoRecord(indexer, schema, analyzer, 0, "Tom Smith and Jack Lennon", "Yesterday Once More", -100.0, 100.0);
	addGeoRecord(indexer, schema, analyzer, 1, "George Harris", "Here comes the sun", 100.0, 100.0);
	addGeoRecord(indexer, schema, analyzer, 2, "George Harris", "Here comes the sun", 1.0, 1.0);
	addGeoRecord(indexer, schema, analyzer, 3, "George Harris", "Here comes the sun", -100.0, -100.0);
	addGeoRecord(indexer, schema, analyzer, 4, "George Harris", "Here comes the sun", 100.0, -100.0);

	// commit the index
    bool retval = indexer->commit();
	ASSERT( retval == 1 );
    (void)retval;

    // Storing results of the query and expected results
    vector<vector<unsigned>*> expectedResults;
    vector<vector<GeoElement*>*>  results;

    QueryEvaluatorRuntimeParametersContainer runTimeParameters;
    QueryEvaluator * queryEvaluator = new QueryEvaluator(indexer,&runTimeParameters );


    //Rectangle queryRange(pair(pair(-20,-20),pair(20,20)));
    Rectangle rectangle;
    rectangle.max.x=20;
    rectangle.max.y=20;
    rectangle.min.x=10;
    rectangle.min.y=10;
    boost::shared_ptr<QuadTreeRootNodeAndFreeLists> quadtree_ReadView;
    quadtree_ReadView = dynamic_cast<IndexReaderWriter *>(indexer)->getQuadTree_ReadView();
    QuadTreeNode *qt = quadtree_ReadView->root;
    qt->rangeQuery(results,rectangle);
    vector<unsigned> res;

    res.push_back(getExternalId(queryEvaluator,0));
    res.push_back(getExternalId(queryEvaluator,1));
    res.push_back(getExternalId(queryEvaluator,2));
    res.push_back(getExternalId(queryEvaluator,3));
    res.push_back(getExternalId(queryEvaluator,4));
    expectedResults.push_back(&res);

    verifyResults(results,expectedResults);

	delete indexer;
    delete indexMetaData;
	delete analyzer;
	delete schema;
}

// Test using the circle range
void testCircleRange(string directoryName)
{
	// Create a schema
	Schema *schema = Schema::create(LocationIndex);
	schema->setPrimaryKey("list_id"); // integer, by default not searchable
	schema->setSearchableAttribute("title", 2); // searchable text
	schema->setSearchableAttribute("address", 7); // searchable text

    // Create an analyzer
    Analyzer *analyzer = new Analyzer(NULL, NULL, NULL, NULL, "");

    unsigned mergeEveryNSeconds = 3;
    unsigned mergeEveryMWrites = 5;
    unsigned updateHistogramEveryPMerges = 1;
    unsigned updateHistogramEveryQWrites = 5;
	CacheManager *cache = new CacheManager(134217728);
    IndexMetaData *indexMetaData = new IndexMetaData( cache,
    		mergeEveryNSeconds, mergeEveryMWrites,
    		updateHistogramEveryPMerges, updateHistogramEveryQWrites,
    		directoryName);

	Indexer *indexer = Indexer::create(indexMetaData, analyzer, schema);

	// Create five records of 8 attributes and add them to the index
	addGeoRecord(indexer, schema, analyzer, 0, "Tom Smith and Jack Lennon", "Yesterday Once More", 100.0, 100.0);
	addGeoRecord(indexer, schema, analyzer, 1, "George Harris", "Here comes the sun", 110.0, 110.0);
	addGeoRecord(indexer, schema, analyzer, 2, "George Harris", "Here comes the sun", 10.0, 10.0);
	addGeoRecord(indexer, schema, analyzer, 3, "George Harris", "Here comes the sun", -100.0, -100.0);
	addGeoRecord(indexer, schema, analyzer, 4, "George Harris", "Here comes the sun", -110.0, -110.0);
	addGeoRecord(indexer, schema, analyzer, 5, "George Harris", "Here comes the sun", -100.0, 100.0);
	addGeoRecord(indexer, schema, analyzer, 6, "George Harris", "Here comes the sun", 100.0, -100.0);
	addGeoRecord(indexer, schema, analyzer, 7, "George Harris", "Here comes the sun", 101.0, -101.0);

	// commit the index
    bool retval = indexer->commit();
	ASSERT( retval == 1 );
    (void)retval;

    // Storing results of the query and expected results
    vector<vector<unsigned>*> expectedResults;
    vector<vector<GeoElement*>*>  results;

    QueryEvaluatorRuntimeParametersContainer runTimeParameters;
    QueryEvaluator * queryEvaluator = new QueryEvaluator(indexer,&runTimeParameters );


    //Rectangle queryRange(pair(pair(-20,-20),pair(20,20)));
    Point point;
    point.x = 100; point.y = 100;
    Circle circle(point,30);
    boost::shared_ptr<QuadTreeRootNodeAndFreeLists> quadtree_ReadView;
    quadtree_ReadView = dynamic_cast<IndexReaderWriter *>(indexer)->getQuadTree_ReadView();
    QuadTreeNode *qt = quadtree_ReadView->root;
    qt->rangeQuery(results,circle);
    vector<unsigned> res;

    res.push_back(getExternalId(queryEvaluator,0));
    res.push_back(getExternalId(queryEvaluator,1));
    res.push_back(getExternalId(queryEvaluator,2));

    expectedResults.push_back(&res);

    verifyResults(results,expectedResults);

	delete indexer;
    delete indexMetaData;
	delete analyzer;
	delete schema;
}

// each record has two searchable attributes
void readRecordsFromFile(Indexer *indexer, Schema *schema, Analyzer* analyzer, const string &directoryName)
{
	string primaryKeysFile = directoryName + "/primaryKeys.txt";
	string pointsFile = directoryName + "/points.txt";
	string firstAttrsFile = directoryName + "/firstAttr.txt";
	string secondAttrsFile = directoryName + "/secondAttr.txt";

	ifstream primaryKeys(primaryKeysFile.c_str());
	ASSERT ( primaryKeys ); // cannot open primaryKeys file

	ifstream points(pointsFile.c_str());
	ASSERT  ( points ); //cannot open points file

	ifstream firstAttrs(firstAttrsFile.c_str());
	ASSERT  ( firstAttrs ); // cannot open firstAttrs file

	ifstream secondAttrs(secondAttrsFile.c_str());
	ASSERT  ( secondAttrs ); //cannot open secondAttrs file

	string primaryKeyLine;
	string pointLine;
	string firstAttrLine;
	string secondAttrLine;

	// inserting objects in the Quadtree
	while (getline(primaryKeys, primaryKeyLine)
			&& getline(points, pointLine)
			&& getline(firstAttrs, firstAttrLine)
			&& getline(secondAttrs, secondAttrLine))
	{
		unsigned primaryKey = 0;
		Point point;
		point.x = 0.0;
		point.y = 0.0;

        bool retval = parseUnsigned(primaryKeyLine, primaryKey);
		ASSERT  ( retval ); //cannot parse primaryKeys file correctly
        retval = parsePoint(pointLine, point);
		ASSERT  ( retval ); //cannot parse points file correctly"
        (void)retval;

		addGeoRecord(indexer, schema, analyzer, primaryKey, firstAttrLine, secondAttrLine, point.x, point.y);
	}
}

void Serialize(string directoryName)
{
	// Create a schema
	Schema *schema = Schema::create(LocationIndex);
	schema->setPrimaryKey("list_id"); // integer, by default not searchable
	schema->setSearchableAttribute("title", 2); // searchable text
	schema->setSearchableAttribute("address", 7); // searchable text

    // Create an analyzer
    Analyzer *analyzer = new Analyzer(NULL, NULL, NULL, NULL, "");

    unsigned mergeEveryNSeconds = 3;
    unsigned mergeEveryMWrites = 5;
    unsigned updateHistogramEveryPMerges = 1;
    unsigned updateHistogramEveryQWrites = 5;
	CacheManager *cache = new CacheManager(134217728);
    IndexMetaData *indexMetaData = new IndexMetaData(cache,
    		mergeEveryNSeconds, mergeEveryMWrites,
    		updateHistogramEveryPMerges, updateHistogramEveryQWrites,
    		directoryName);

	Indexer *indexer = Indexer::create(indexMetaData, analyzer, schema);

	readRecordsFromFile(indexer, schema, analyzer, directoryName+"/quadtree/1K");

    boost::shared_ptr<QuadTreeRootNodeAndFreeLists> quadtree_ReadView;
    quadtree_ReadView = dynamic_cast<IndexReaderWriter *>(indexer)->getQuadTree_ReadView();
    QuadTreeNode *qt = quadtree_ReadView->root;

	// serialize the index
	indexer->commit();
	indexer->save(directoryName);

	delete indexer;
    delete indexMetaData;
	delete analyzer;
	delete schema;
}

void testDeserialization(string directoryName)
{
	Serialize(directoryName);
	Schema *schema = Schema::create(LocationIndex);
	schema->setPrimaryKey("list_id"); // integer, by default not searchable
	schema->setSearchableAttribute("title", 2); // searchable text
	schema->setSearchableAttribute("address", 7); // searchable text
    Analyzer *analyzer = new Analyzer(NULL, NULL, NULL, NULL, "");
    unsigned mergeEveryNSeconds = 3;
    unsigned mergeEveryMWrites = 5;
    unsigned updateHistogramEveryPMerges = 1;
    unsigned updateHistogramEveryQWrites = 5;
	CacheManager *cache = new CacheManager(134217728);
    IndexMetaData *indexMetaData = new IndexMetaData(cache,
    		mergeEveryNSeconds, mergeEveryMWrites,
    		updateHistogramEveryPMerges, updateHistogramEveryQWrites,
    		directoryName);

	// load the quadtree from disk
    Indexer *indexer1 = Indexer::load(indexMetaData);
    boost::shared_ptr<QuadTreeRootNodeAndFreeLists> quadtree_ReadView;
    quadtree_ReadView = dynamic_cast<IndexReaderWriter *>(indexer1)->getQuadTree_ReadView();
    QuadTreeNode *qt1 = quadtree_ReadView->root;

	// rebuild the old quadtree
	Indexer *indexer2 = Indexer::create(indexMetaData, analyzer, schema);
	readRecordsFromFile(indexer2, schema, analyzer, directoryName+"/quadtree/1K");
	indexer2->commit();
    boost::shared_ptr<QuadTreeRootNodeAndFreeLists> quadtree_ReadView2;
    quadtree_ReadView2 = dynamic_cast<IndexReaderWriter *>(indexer2)->getQuadTree_ReadView();
    QuadTreeNode *qt2 = quadtree_ReadView->root;

	ASSERT(qt1->getNumOfElementsInSubtree() == qt2->getNumOfElementsInSubtree());
	ASSERT(qt1->getNumOfLeafNodesInSubtree() == qt2->getNumOfLeafNodesInSubtree());

	// test if the loaded quadtree is exactly the same as the old one
	bool isEqual = false;
	if(qt1->equalTo(qt2) && qt2->equalTo(qt1))
		isEqual = true;
	ASSERT( isEqual == true );

	delete indexer2;
	delete indexer1;
	delete indexMetaData;
	delete analyzer;
	delete schema;
}


int main(int argc, char *argv[])
{

	bool verbose = false;
	if ( argc > 1 && strcmp(argv[1], "--verbose") == 0) {
		verbose = true;
	}

	//unsigned threshold = atoi(argv[1]);

	const string directoryName = getenv("directoryName");
	//string directoryName = "../test/unit/test_data";

	// Test the case where we only have one node(root) with a few records in the tree
	testSingleNodeQuadTree(directoryName);
    cout << "1/4 testSingleNodeQuadTree passes." << endl;

	// Test circle range
	testCircleRange(directoryName);
    cout << "2/4 testCircleRange passes." << endl;

	//--- Serialization ---//

    testDeserialization(directoryName);
    cout << "3/4 testSerialization passes" << endl;
    cout << "4/4 testDeserialization passes." << endl;


	cout << "All Quadtree tests passed!" << endl;

	return 0;
}


