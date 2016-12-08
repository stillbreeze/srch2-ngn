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

#ifndef __WRAPPER_LOGICALPLAN_H__
#define __WRAPPER_LOGICALPLAN_H__

#include "Constants.h"
#include "instantsearch/Term.h"
#include "instantsearch/ResultsPostProcessor.h"
#include "util/Assert.h"
#include "record/LocationRecordUtil.h"
#include <vector>
#include <string>
#include <sstream>
using namespace std;

namespace srch2 {
namespace instantsearch {

class LogicalPlanNodeAnnotation;
/*
 * LogicalPlanNode is the common class used for the logical plan tree operators and operands. And tree is
 * constructed by attaching the instances of this class and the pointer to the root is kept in LogicalPlan.
 */
class LogicalPlanNode{
	// Since constructor is private, only LogicalPlan can allocate space for LogicalPlanNodes.
	friend class LogicalPlan;
public:
	LogicalPlanNodeType nodeType;
	vector<LogicalPlanNode *> children;
	Term * exactTerm;
	Term * fuzzyTerm;
	Shape* regionShape;
	LogicalPlanNodeAnnotation * stats;

	PhysicalPlanNodeType forcedPhysicalNode;

	virtual ~LogicalPlanNode();

    void setFuzzyTerm(Term * fuzzyTerm);

    unsigned getNumberOfLeafNodes(){
    	if(nodeType == LogicalPlanNodeTypeTerm){
    		return 1;
    	}else{
    		unsigned sumOfChildren = 0;
    		for(vector<LogicalPlanNode *>::iterator child = children.begin(); child != children.end() ; ++child){
    			sumOfChildren += (*child)->getNumberOfLeafNodes();
    		}
    		return sumOfChildren;
    	}
    }

    virtual string toString();
    string getSubtreeUniqueString();
    Term * getTerm(bool isFuzzy){
    	if(isFuzzy){
    		return this->fuzzyTerm;
    	}else{
    		return this->exactTerm;
    	}
    }

protected:
	LogicalPlanNode(Term * exactTerm, Term * fuzzyTerm);
	LogicalPlanNode(LogicalPlanNodeType nodeType);
	LogicalPlanNode(Shape* regionShape);
};

class LogicalPlanPhraseNode : public LogicalPlanNode{
public:
	LogicalPlanPhraseNode(const vector<string>& phraseKeyWords,
	    		const vector<unsigned>& phraseKeywordsPosition,
	    		short slop, const vector<unsigned>& fieldFilter,
                        ATTRIBUTES_OP attrOp) : LogicalPlanNode(LogicalPlanNodeTypePhrase) {
		phraseInfo = new PhraseInfo();
		phraseInfo->attributeIdsList = fieldFilter;
		phraseInfo->phraseKeyWords = phraseKeyWords;
		phraseInfo->phraseKeywordPositionIndex = phraseKeywordsPosition;
		phraseInfo->proximitySlop = slop;
		phraseInfo->attrOps = attrOp; 
	}
	string toString() {
		string key = LogicalPlanNode::toString();
		if (phraseInfo) {
			key += phraseInfo->toString();
		}
		return key;
	}
	~LogicalPlanPhraseNode() { delete phraseInfo;}
	PhraseInfo * getPhraseInfo() { return phraseInfo; }
private:
	PhraseInfo *phraseInfo;
};
/*
 * LogicalPlan is the class which maintains the logical plan of query and its metadata. The logical plan is
 * a tree of operators (AND,OR and NOT) and query terms. For example, for the query:
 * q=(John AND hello*) OR (authors:Kundera AND freedom~0.5)
 * the logical plan is :
 *
 *   [OR]_____[AND]_____{John}
 *    |         |
 *    |         |_______{hello, TermType=PREFIX}
 *    |
 *    |_______[AND]_____{Kundera, fieldFilter=Authors}
 *              |
 *              |_______{freedom, similarityThreshold=0.5}
 *
 * Each node in this tree is a LogicalPlanNode.
 * Note: The shapes of logical plan and parse tree are similar. The difference between parse-tree and
 * logical plan is:
 * 1. Logical plan is made by traversing the parse tree. So parse tree is a product which is made before
 *    logical plan from the query
 * 2. LogicalPlan objects keep other information like searchType in addition to the query tree
 */
class LogicalPlan {
private:
	// SearchType
	// PostProcessingInfo
    LogicalPlanNode * tree;
    vector<string> attributesToReturn;

public:
    LogicalPlan();
    ~LogicalPlan();

    void setAttrToReturn(const vector<string>& attr){
        attributesToReturn = attr;
    }

    const vector<string> getAttrToReturn() const{
        return this->attributesToReturn;
    }

	std::string docIdForRetrieveByIdSearchType;
	ResultsPostProcessorPlan * postProcessingPlan;
	ResultsPostProcessingInfo * postProcessingInfo;
	/// Plan related information
	srch2::instantsearch::QueryType queryType;
	// the offset of requested results in the result set
	int offset;
	int numberOfResultsToRetrieve;
	bool shouldRunFuzzyQuery;
	Query *exactQuery;
	Query *fuzzyQuery;
	string queryStringWithTermsAndOps;
    // constructs a term logical plan node
    LogicalPlanNode * createTermLogicalPlanNode(const std::string &queryKeyword,
    		TermType type,
    		const float boost,
    		const float similarityBoost,
    		const uint8_t threshold,
    		const vector<unsigned>& fieldFilter, ATTRIBUTES_OP atrOps);
    // constructs an internal (operator) logical plan node
    LogicalPlanNode * createOperatorLogicalPlanNode(LogicalPlanNodeType nodeType);
    LogicalPlanNode * createPhraseLogicalPlanNode(const vector<string>& phraseKeyWords,
    		const vector<unsigned>& phraseKeywordsPosition,
    		short slop, const vector<unsigned>& fieldFilter, ATTRIBUTES_OP attrOp) ;

	ResultsPostProcessorPlan* getPostProcessingPlan() const {
		return postProcessingPlan;
	}

	void setPostProcessingPlan(ResultsPostProcessorPlan* postProcessingPlan) {
		this->postProcessingPlan = postProcessingPlan;
	}

	ResultsPostProcessingInfo* getPostProcessingInfo() {
		return postProcessingInfo;
	}

	void setPostProcessingInfo(ResultsPostProcessingInfo* postProcessingInfo) {
		this->postProcessingInfo = postProcessingInfo;
	}

	LogicalPlanNode * getTree(){
		return tree;
	}

	const LogicalPlanNode * getTreeForRead() const{
		return tree;
	}
	void setTree(LogicalPlanNode * tree){
		this->tree = tree;
	}

	// if this function returns true we must use fuzzy search
	// if we dont find enough exact results;
	bool isFuzzy() const {
		return shouldRunFuzzyQuery;
	}

	void setFuzzy(bool isFuzzy) {
		this->shouldRunFuzzyQuery = isFuzzy;
	}

	int getOffset() const {
		return offset;
	}

	void setOffset(int offset) {
		this->offset = offset;
	}

	int getNumberOfResultsToRetrieve() const {
		return numberOfResultsToRetrieve;
	}

	void setNumberOfResultsToRetrieve(int resultsToRetrieve) {
		this->numberOfResultsToRetrieve = resultsToRetrieve;
	}

	srch2is::QueryType getQueryType() const {
		return queryType;
	}
	Query* getExactQuery() const{
		return exactQuery;
	}

	void setExactQuery(Query* exactQuery) {
		// it gets enough information from the arguments and allocates the query object
        if(this->exactQuery == NULL){
            this->exactQuery = exactQuery;
        }else{
        	ASSERT(false);
        }
	}

	Query* getFuzzyQuery() {
		return fuzzyQuery;
	}

	void setFuzzyQuery(Query* fuzzyQuery) {

		// it gets enough information from the arguments and allocates the query object
	    if(this->fuzzyQuery == NULL){
            this->fuzzyQuery = fuzzyQuery;
	    }else{
	    	ASSERT(false);
	    }
	}

	void setQueryType(srch2is::QueryType queryType) {
		this->queryType = queryType;
	}

	std::string getDocIdForRetrieveByIdSearchType(){
		return this->docIdForRetrieveByIdSearchType;
	}

	void setDocIdForRetrieveByIdSearchType(const std::string & docid){
		this->docIdForRetrieveByIdSearchType = docid;
	}

	LogicalPlanNode * createGeoLogicalPlanNode(Shape *regionShape);


	/*
	 * This function returns a string representation of the logical plan
	 * by concatenating different parts together. The call to getSubtreeUniqueString()
	 * gives us a tree representation of the logical plan tree. For example is query is
	 * q = FOO1 AND BAR OR FOO2
	 * the string of this subtree is something like:
	 * FOO1_BAR_FOO2_OR_AND
	 */
	string getUniqueStringForCaching(){
		stringstream ss;
		if(tree != NULL){
			ss << tree->getSubtreeUniqueString().c_str();
		}
		ss << docIdForRetrieveByIdSearchType;
		if(postProcessingInfo != NULL){
			ss << postProcessingInfo->toString().c_str();
		}
		ss << queryType;
		ss << offset;
		ss << numberOfResultsToRetrieve;
		ss << shouldRunFuzzyQuery;
		if(exactQuery != NULL){
			ss << exactQuery->toString().c_str();
		}
		if(fuzzyQuery != NULL){
			ss << fuzzyQuery->toString().c_str();
		}
		return ss.str();
	}
};

}
}

#endif // __WRAPPER_LOGICALPLAN_H__
