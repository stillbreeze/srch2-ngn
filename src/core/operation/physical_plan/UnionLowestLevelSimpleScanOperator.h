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


#ifndef __WRAPPER_UNIONLOWESTLEVELSUMPLESCANOPERATOR_H__
#define __WRAPPER_UNIONLOWESTLEVELSUMPLESCANOPERATOR_H__

#include "instantsearch/Constants.h"
#include "index/ForwardIndex.h"
#include "index/Trie.h"
#include "index/InvertedIndex.h"
#include "operation/HistogramManager.h"
#include "PhysicalPlan.h"

using namespace std;

namespace srch2 {
namespace instantsearch {

/*
 * This operator is a simple scan operator. It starts with a set of activenodes,
 * finds all the leaf nodes based on the term type (prefix or complete),
 * saves all the connected inverted lists, and then in each getNext call returns
 * one record from a list until all lists are iterated completely.
 * The difference between this operator and TVL is that this operator doesn't give
 * any guarantee of order for its output (the output is NOT sorted by score) and
 * it doesn't have the overhead of HEAP that TVL has.
 */
class UnionLowestLevelSimpleScanOperator : public PhysicalPlanNode {
	friend class PhysicalOperatorFactory;
public:
	bool open(QueryEvaluatorInternal * queryEvaluator, PhysicalPlanExecutionParameters & params);
	PhysicalPlanRecordItem * getNext(const PhysicalPlanExecutionParameters & params) ;
	bool close(PhysicalPlanExecutionParameters & params);
	string toString();
	bool verifyByRandomAccess(PhysicalPlanRandomAccessVerificationParameters & parameters) ;
	~UnionLowestLevelSimpleScanOperator();
private:
	UnionLowestLevelSimpleScanOperator() ;

	void depthInitializeSimpleScanOperator(
			const TrieNode* trieNode,const TrieNode* prefixNode, unsigned editDistance, unsigned panDistance, unsigned bound);

    //this flag is set to true when the parent is feeding this operator
    // a cache entry and expects a newer one in close()
    bool parentIsCacheEnabled;

	QueryEvaluatorInternal * queryEvaluator;
	shared_ptr<vectorview<InvertedListContainerPtr> > invertedListDirectoryReadView;
    shared_ptr<vectorview<unsigned> > invertedIndexKeywordIdsReadView;
	shared_ptr<vectorview<ForwardListPtr> >  forwardIndexDirectoryReadView;
	vector< vectorview<unsigned> * > invertedLists;
	vector< shared_ptr<vectorview<unsigned> > > invertedListsSharedPointers;
	vector< unsigned > invertedListDistances;
	vector< TrieNodePointer > invertedListPrefixes;
	vector< TrieNodePointer > invertedListLeafNodes;
	vector<unsigned> invertedListIDs;
	unsigned invertedListOffset;
	unsigned cursorOnInvertedList;
};

class UnionLowestLevelSimpleScanCacheEntry : public PhysicalOperatorCacheObject {
public:
	unsigned invertedListOffset;
	unsigned cursorOnInvertedList;
	UnionLowestLevelSimpleScanCacheEntry(	unsigned invertedListOffset,
										unsigned cursorOnInvertedList){
		this->invertedListOffset = invertedListOffset;
		this->cursorOnInvertedList = cursorOnInvertedList;
	}

    unsigned getNumberOfBytes() {
    	return sizeof(UnionLowestLevelSimpleScanCacheEntry);
    }

	~UnionLowestLevelSimpleScanCacheEntry(){
	}
};

class UnionLowestLevelSimpleScanOptimizationOperator : public PhysicalPlanOptimizationNode {
	friend class PhysicalOperatorFactory;
public:
	// The cost of open of a child is considered only once in the cost computation
	// of parent open function.
	PhysicalPlanCost getCostOfOpen(const PhysicalPlanExecutionParameters & params) ;
	// The cost of getNext of a child is multiplied by the estimated number of calls to this function
	// when the cost of parent is being calculated.
	PhysicalPlanCost getCostOfGetNext(const PhysicalPlanExecutionParameters & params) ;
	// the cost of close of a child is only considered once since each node's close function is only called once.
	PhysicalPlanCost getCostOfClose(const PhysicalPlanExecutionParameters & params) ;
	PhysicalPlanCost getCostOfVerifyByRandomAccess(const PhysicalPlanExecutionParameters & params);
	void getOutputProperties(IteratorProperties & prop);
	void getRequiredInputProperties(IteratorProperties & prop);
	PhysicalPlanNodeType getType() ;
	bool validateChildren();
};

}
}

#endif // __WRAPPER_UNIONLOWESTLEVELSUMPLESCANOPERATOR_H__
