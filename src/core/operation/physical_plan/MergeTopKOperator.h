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

#ifndef __WRAPPER_MERGETOPKOPERATOR_H__
#define __WRAPPER_MERGETOPKOPERATOR_H__

#include "instantsearch/Constants.h"
#include "index/ForwardIndex.h"
#include "index/Trie.h"
#include "index/InvertedIndex.h"
#include "operation/HistogramManager.h"
#include "PhysicalPlan.h"

#include <boost/unordered_set.hpp>

using namespace std;

namespace srch2 {
namespace instantsearch {


class PhysicalPlanRecordItemComparator
{
public:

  // descending order
  bool operator() (const PhysicalPlanRecordItem*  lhs, const PhysicalPlanRecordItem*  rhs) const
  {
      float leftRecordScore, rightRecordScore;
      unsigned leftRecordId  = lhs->getRecordId();
      unsigned rightRecordId = rhs->getRecordId();
		float _leftRecordScore = lhs->getRecordRuntimeScore();
		float _rightRecordScore = rhs->getRecordRuntimeScore();
		return DefaultTopKRanker::compareRecordsGreaterThan(_leftRecordScore,  leftRecordId,
		                                    _rightRecordScore, rightRecordId);
  }
};



class MergeTopKCacheEntry : public PhysicalOperatorCacheObject {
public:
	vector<PhysicalPlanRecordItem *> candidatesList;
	vector<PhysicalPlanRecordItem *> nextItemsFromChildren;
	boost::unordered_set<unsigned> visitedRecords;
	bool listsHaveMoreRecordsInThem;
	unsigned childRoundRobinOffset;

	MergeTopKCacheEntry(	QueryEvaluatorInternal * queryEvaluator,
			                vector<PhysicalPlanRecordItem *> candidatesList,
							vector<PhysicalPlanRecordItem *> nextItemsFromChildren,
							boost::unordered_set<unsigned> visitedRecords,
							bool listsHaveMoreRecordsInThem,
							unsigned childRoundRobinOffset){
		for(unsigned i = 0; i < candidatesList.size() ; ++i){
			this->candidatesList.push_back(queryEvaluator->getPhysicalPlanRecordItemPool()->
					cloneForCache(candidatesList.at(i)));
		}

		for(unsigned i = 0; i < nextItemsFromChildren.size() ; ++i){
			if(nextItemsFromChildren.at(i) == NULL){
				this->nextItemsFromChildren.push_back(NULL);
			}else{
				this->nextItemsFromChildren.push_back(queryEvaluator->getPhysicalPlanRecordItemPool()->
						cloneForCache(nextItemsFromChildren.at(i)));
			}
		}

		this->visitedRecords = visitedRecords;
		this->listsHaveMoreRecordsInThem = listsHaveMoreRecordsInThem;
		this->childRoundRobinOffset = childRoundRobinOffset;
	}

    unsigned getNumberOfBytes() {
    	unsigned numberOfBytes = 0;
    	numberOfBytes += sizeof(MergeTopKCacheEntry);

    	// candidateList
    	numberOfBytes += candidatesList.capacity() * sizeof(PhysicalPlanRecordItem *);
    	for(unsigned i = 0 ; i < candidatesList.size() ; ++i){
    		ASSERT(candidatesList.at(i) != NULL);
    		if(candidatesList.at(i) != NULL){ // just for safety. These pointers shouldn't be ever null
    			numberOfBytes += candidatesList.at(i)->getNumberOfBytes();
    		}
    	}

    	// nextItemsFromChildren
    	numberOfBytes += nextItemsFromChildren.capacity() * sizeof(PhysicalPlanRecordItem *);
    	for(unsigned i = 0 ; i < nextItemsFromChildren.size() ; i++){
    		if(nextItemsFromChildren.at(i) != NULL){
				numberOfBytes += nextItemsFromChildren.at(i)->getNumberOfBytes();
    		}
    	}

    	// visited records
    	// 1.25 is because there must be a fudge factor in unordered_set implementation
    	// and since we assume unordered_set has a bucket implementation this fudge factor
    	// should be about %80. so 1/%80 is 1.25.
    	numberOfBytes += sizeof(unsigned) * visitedRecords.size() * 1.25;
    	for(unsigned childOffset = 0 ; childOffset < children.size() ; ++childOffset){
    		if(children.at(childOffset) != NULL){
				numberOfBytes += children.at(childOffset)->getNumberOfBytes();
    		}
    	}
    	return numberOfBytes;
    }

	~MergeTopKCacheEntry(){
		for(unsigned i = 0; i < candidatesList.size() ; ++i){
			delete candidatesList.at(i);
		}
		for(unsigned i=0; i < nextItemsFromChildren.size() ; ++i){
			delete nextItemsFromChildren.at(i);
		}
	}
};



/*
 * This operator uses the Threshold Algorithm (Fagin's Algorithm) to find the best record of
 * its subtree. Every time getNext is called threshold algorithm is used and it retrieves
 * records by calling getNext of children to find and return the best remaining record in the subtree.
 * The assumption of this operator is that input is sorted based on score (monotonicity property of Fagin's algorithm)
 * This operator is used in the most popular physical plan for instant search :
 * Example :
 * q = A AND B AND C
 *
 * [MergeTopKOperator]_____ [TVL A]
 *        |
 *        |_____ [TVL B]
 *        |
 *        |_____ [TVL C]
 */
class MergeTopKOperator : public PhysicalPlanNode {
	friend class PhysicalOperatorFactory;
public:
	bool open(QueryEvaluatorInternal * queryEvaluator, PhysicalPlanExecutionParameters & params);
	PhysicalPlanRecordItem *
	getNext(const PhysicalPlanExecutionParameters & params) ;
	bool close(PhysicalPlanExecutionParameters & params);
	string toString();
	bool verifyByRandomAccess(PhysicalPlanRandomAccessVerificationParameters & parameters) ;
	~MergeTopKOperator();
private:

	QueryEvaluatorInternal * queryEvaluator;

	FeedbackRanker* feedbackRanker;

	shared_ptr<vectorview<ForwardListPtr> > forwardListDirectoryReadView;

	/* this vector always contains the next record coming out of children
	* this means each record first goes to this vector and then it can be used
	* by the operator.
	* getNextRecordOfChild(unsigned childOffset, parameters) returns the record
	* and makes sure to populate the vector again from the child operator.
	*/
	vector<PhysicalPlanRecordItem *> nextItemsFromChildren;

	/*
	 * This vector keeps the candidates that we have visited so far, and might become
	 * top results in the next calls of getNext
	 */
	vector<PhysicalPlanRecordItem *> candidatesList;
	vector<PhysicalPlanRecordItem *> fullCandidatesListForCache;
	/*
	 * This vector keeps all the records that have been visited so far (including
	 * the returned ones and the candidates and even those records which are not verified)
	 */
	boost::unordered_set<unsigned> visitedRecords; // TODO :  THIS MIGHT BE A BOTTLENECK, maybe we should change it to hash table ?

	/*
	 * This variable is true unless one of the children lists become empty
	 */
	bool listsHaveMoreRecordsInThem;

	PhysicalPlanRecordItem * getNextRecordOfChild(unsigned childOffset , const PhysicalPlanExecutionParameters & params);

	/*
	 * This function does the first call to getNext of all the children and puts the results in
	 * the nextItemsFromChildren vector. This vector is used later by getNextRecordOfChild(...)
	 */
	void initializeNextItemsFromChildren(PhysicalPlanExecutionParameters & params , unsigned fromIndex = 0);


	unsigned childRoundRobinOffset;
	unsigned getNextChildForSequentialAccess();

	bool verifyRecordWithChildren(PhysicalPlanRecordItem * recordItem, unsigned childOffset ,
						std::vector<float> & runTimeTermRecordScores,
						std::vector<float> & staticTermRecordScores,
						std::vector<TrieNodePointer> & termRecordMatchingKeywords,
						std::vector<vector<unsigned> > & attributeIdsList,
						std::vector<unsigned> & prefixEditDistances,
						std::vector<unsigned> & positionIndexOffsets,
						std::vector<TermType>& termTypes,
						const PhysicalPlanExecutionParameters & params);

	bool getMaximumScoreOfUnvisitedRecords(float & score, float maxFeedbackBoostForQuery);

	MergeTopKOperator() ;
	void insertResult(PhysicalPlanRecordItem * recordItem);
	bool hasTopK(const float maxScoreForUnvisitedRecords);
};

class MergeTopKOptimizationOperator : public PhysicalPlanOptimizationNode {
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

#endif //__WRAPPER_MERGETOPKOPERATOR_H__
