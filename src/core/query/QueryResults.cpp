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
 */

#include "QueryResultsInternal.h"
#include "operation/QueryEvaluatorInternal.h"
#include "util/RecordSerializerUtil.h"

using srch2::util::Logger;
namespace srch2
{
namespace instantsearch
{

QueryResultFactory::QueryResultFactory(){
	impl = new QueryResultFactoryInternal();
}
QueryResultFactory::~QueryResultFactory(){
    delete impl;
}

QueryResults::QueryResults(){
	impl = new QueryResultsInternal();
}

void QueryResults::init(QueryResultFactory * resultsFactory, const QueryEvaluator* queryEvaluator, Query* query){
	impl->init(resultsFactory,queryEvaluator->impl,query);
}

/**
 * Creates a QueryResults object.
 * @param[in] indexSearcher the reference to an IndexSearcher object.
 * @param[in] query the reference to a Query object.
 */
QueryResults::QueryResults(QueryResultFactory * resultsFactory, const  QueryEvaluator* queryEvaluator, Query* query){
	impl = new QueryResultsInternal(resultsFactory,queryEvaluator->impl,query );
}

QueryResults::~QueryResults(){
	delete impl;
}

/**
 * Checks if the iterator reaches the end. @return false if
 * there is no next item in the QueryResults.
 */
/*virtual bool next() = 0;*/

/**
 * Gets the number of result items in the QueryResults object.
 */
unsigned QueryResults::getNumberOfResults() const{
	 return impl->sortedFinalResults.size();
}

/**
 * Gets the current record id while iterating through the QueryResults object.
 */
/*virtual int getRecordId() = 0;*/

/**
 * Gets the record id of the 'position'-th item in the QueryResults object.
 */
std::string QueryResults::getRecordId(unsigned position) const {
    ASSERT (position < this->getNumberOfResults());
    return impl->sortedFinalResults.at(position)->externalRecordId;
}

/**
 * Gets the record id of the 'position'-th item in the QueryResults object.
 * TODO: Move/remove getInternalRecordId to internal include files. There should be a mapping from external
 * Used to access the InMemoryData.
 */
unsigned QueryResults::getInternalRecordId(unsigned position) const {
    ASSERT (position < this->getNumberOfResults());
    return impl->sortedFinalResults.at(position)->internalRecordId;
}

/*
 *   this function is called only from unit test. Do not use it in wrapper layer.
 */
std::string QueryResults::getInMemoryRecordString_Safe(unsigned position) const {
    unsigned internalRecordId = this->getInternalRecordId(position);
    StoredRecordBuffer buffer = impl->queryEvaluatorInternal->getInMemoryData_Safe(internalRecordId);
    string inMemoryString = "";
    if (!buffer.start)
    	inMemoryString = string(buffer.start.get(), buffer.length);
    return inMemoryString;
}

/**
 * Gets the score of the 'position'-th item in the QueryResults object.
 */
std::string QueryResults::getResultScoreString(unsigned position) const {
    ASSERT (position < this->getNumberOfResults());
    return impl->sortedFinalResults.at(position)->_score.toString();
}
TypedValue QueryResults::getResultScore(unsigned position) const {
    ASSERT (position < this->getNumberOfResults());
    return impl->sortedFinalResults.at(position)->_score;
}

/**
 *
 * Gets the matching keywords of the 'position'-th item in
 * the QueryResults object. For each query keyword return a matching
 * keyword in a record. If the query keyword is prefix then return a
 * matching prefix. In the case of multiple matching keywords for a
 * query keyword,the best match based on edit distance is returned.
 *
 * For example, for the query "ulman compilor", a result
 * record with keywords "ullman principles compiler" will
 * return ["ullman","compiler"] in the matchingKeywords
 * vector.  In particular, "compiler" is the best matching
 * keyword in the record for the second query keyword
 * "compilor".
 */
void QueryResults::getMatchingKeywords(const unsigned position, std::vector<std::string> &matchingKeywords) const {
    matchingKeywords.assign(impl->sortedFinalResults[position]->matchingKeywords.begin(),
    		impl->sortedFinalResults[position]->matchingKeywords.end());
}

/**
 * Gets the edit distances of the 'position'-th item in the
 * QueryResults object.
 * @param[in] position the position of a result in the returned
 * results (starting from 0).
 * @param[out] editDistances a vector of edit distances of the
 * best matching keywords in this result record with respect
 * to the query keywords.
 *
 * For example, for the query "ulman compilor", a result
 * recorprivate:
struct Impl;
Impl *impl;d with keywords "ullman principles conpiler" will
 * return [1,2] in the editDistances vector.  In particular,
 * "2" means that the best matching keyword ("conpiler") in
 * this record has an edit distance 2 to the second query
 * keyword "compilor".
 */
void QueryResults::getEditDistances(const unsigned position, std::vector<unsigned> &editDistances) const {
    editDistances.assign(impl->sortedFinalResults[position]->editDistances.begin(),
    		impl->sortedFinalResults[position]->editDistances.end());
}


void QueryResults::getMatchedAttributes(const unsigned position, std::vector<std::vector<unsigned> > &matchedAttributes) const {
	//TODO opt
	const vector<vector<unsigned> > &matchedAttributeIdsList = impl->sortedFinalResults[position]->attributeIdsList;
	matchedAttributes.resize(matchedAttributeIdsList.size());

	for(int i = 0; i < matchedAttributeIdsList.size(); i++)
	{
		unsigned idx = 0;
		const vector<unsigned>& matchedAttributeIds = matchedAttributeIdsList[i];
		matchedAttributes[i].clear();
		for (unsigned j =0; j < matchedAttributeIds.size(); ++j)
		{
			matchedAttributes[i].push_back(matchedAttributeIds[j]);
		}
	}
}

void QueryResults::getTermTypes(unsigned position, vector<TermType>& tt) const{
	tt.assign(impl->sortedFinalResults[position]->termTypes.begin(),
    		impl->sortedFinalResults[position]->termTypes.end());
}

/*
 *   In Geo search return distance between location of the result and center of the query rank.
 *   TODO: Change the name to getGeoDistance()
 *   // only the results of MapQuery have this
 */
double QueryResults::getPhysicalDistance(const unsigned position) const {
    ASSERT (position < this->getNumberOfResults());
    return impl->sortedFinalResults.at(position)->physicalDistance;
}

const std::map<std::string, std::pair< FacetType , std::vector<std::pair<std::string, float> > > > * QueryResults::getFacetResults() const{
	return &impl->facetResults;
}


// pass information to the destination QueryResults object
// can be seen as this := sourceQueryResults (only for structures related to post processing)
void QueryResults::copyForPostProcessing(QueryResults * sourceQueryResults) const {
    this->impl->sortedFinalResults = sourceQueryResults->impl->sortedFinalResults ;
    this->impl->facetResults = sourceQueryResults->impl->facetResults ;
    this->impl->resultsApproximated = sourceQueryResults->impl->resultsApproximated;
    this->impl->estimatedNumberOfResults = sourceQueryResults->impl->estimatedNumberOfResults;
}

void QueryResults::clear(){

	this->impl->sortedFinalResults.clear();
	this->impl->facetResults.clear();

}

bool QueryResults::isResultsApproximated() const{
	return this->impl->resultsApproximated;
}

long int QueryResults::getEstimatedNumberOfResults() const{
	return this->impl->estimatedNumberOfResults;
}

//TODO: These three functions for internal debugging. remove from the header
void QueryResults::printStats() const {
    impl->stat->print();
}

void QueryResults::printResult() const {
	// show attributeBitmaps
	vector<vector<unsigned> > attributes;
	vector<string> matchedKeywords;
    Logger::debug("Result count %d" ,this->getNumberOfResults());
	for(int i = 0; i < this->getNumberOfResults(); i++)
	{
        Logger::debug("Result #%d" ,i);
		this->getMatchingKeywords(i, matchedKeywords);
		this->getMatchedAttributes(i, attributes);
		for(int j = 0; j < attributes.size(); j++)
		{
			for(int k = 0; k < attributes[j].size(); k++)
				Logger::debug("%d", attributes[j][k]);
            Logger::debug("}");
		}

	}

}

void QueryResults::addMessage(const char* msg) {
	 impl->stat->addMessage(msg);
}


}}
