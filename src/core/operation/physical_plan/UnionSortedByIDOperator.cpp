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

#include "PhysicalOperators.h"
#include "PhysicalOperatorsHelper.h"

namespace srch2 {
namespace instantsearch {

////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////// Union lists sorted by ID /////////////////////////////////////

UnionSortedByIDOperator::UnionSortedByIDOperator() {}
UnionSortedByIDOperator::~UnionSortedByIDOperator(){}

bool UnionSortedByIDOperator::open(QueryEvaluatorInternal * queryEvaluator, PhysicalPlanExecutionParameters & params){
	/*
	 * 1. open all children (no parameters known to pass as of now)
	 * 2. initialize nextRecordItems vector.
	 */
	unsigned numberOfChildren = this->getPhysicalPlanOptimizationNode()->getChildrenCount();
	for(unsigned childOffset = 0 ; childOffset < numberOfChildren ; ++childOffset){
		this->getPhysicalPlanOptimizationNode()->getChildAt(childOffset)->getExecutableNode()->open(queryEvaluator , params);
	}

	for(unsigned childOffset = 0; childOffset < numberOfChildren; ++childOffset){
		PhysicalPlanRecordItem * recordItem =
				this->getPhysicalPlanOptimizationNode()->getChildAt(childOffset)->getExecutableNode()->getNext(params);
		this->nextItemsFromChildren.push_back(recordItem);
	}

	listsHaveMoreRecordsInThem = true;
	visitedRecords.clear();
	return true;

}
PhysicalPlanRecordItem * UnionSortedByIDOperator::getNext(const PhysicalPlanExecutionParameters & params) {

	if(listsHaveMoreRecordsInThem == false){
		return NULL;
	}

	/*
	 * 0. Iterate on nextItemsFromChildren and getNext until an unvisited
	 * ---- record comes up
	 * 1. iterate on nextItemsFromChildren
	 * 2. find the min ID on top,
	 * 3. remove all top records that have the same ID as min ID
	 * 4. return the min ID (which has the most Score in ties)
	 */
	unsigned numberOfChildren = this->getPhysicalPlanOptimizationNode()->getChildrenCount();

	for(unsigned childOffset = 0; childOffset < numberOfChildren; ++childOffset){
		while(true){
			PhysicalPlanRecordItem * recordItem = this->nextItemsFromChildren.at(childOffset);
			if(recordItem == NULL){
				break;;
			}
			if(find(this->visitedRecords.begin(), this->visitedRecords.end(), recordItem->getRecordId()) == this->visitedRecords.end()){
				break;
			}else{
				this->nextItemsFromChildren.at(childOffset) =
						this->getPhysicalPlanOptimizationNode()->getChildAt(childOffset)->getExecutableNode()->getNext(params);
			}
		}
	}

	// find the min ID
	PhysicalPlanRecordItem * minIDRecord = NULL;
	for(unsigned childOffset = 0; childOffset < numberOfChildren; ++childOffset){
		PhysicalPlanRecordItem * recordItem = this->nextItemsFromChildren.at(childOffset);
		if(recordItem == NULL){
			continue;
		}
		if(minIDRecord == NULL){
			minIDRecord = recordItem;
		}else if((minIDRecord->getRecordId() > recordItem->getRecordId()) // find the 1. min ID and 2. max score
				|| (minIDRecord->getRecordId() == recordItem->getRecordId() &&
						minIDRecord->getRecordRuntimeScore() < recordItem->getRecordRuntimeScore())){
			minIDRecord = recordItem;
		}

	}
	if(minIDRecord == NULL){
		this->listsHaveMoreRecordsInThem = false;
		return NULL;
	}
	// now remove all minID records from all lists
	vector<float> runtimeScores;
	vector<TrieNodePointer> recordKeywordMatchPrefixes;
	vector<unsigned> recordKeywordMatchEditDistances;
	vector<vector<unsigned> > matchedAttributeIdsList;
	vector<unsigned> positionIndexOffsets;
	vector<TermType> termTypes;
	for(unsigned childOffset = 0; childOffset < numberOfChildren; ++childOffset){
		if(this->nextItemsFromChildren.at(childOffset) != NULL &&
				this->nextItemsFromChildren.at(childOffset)->getRecordId() == minIDRecord->getRecordId()){
			runtimeScores.push_back( this->nextItemsFromChildren.at(childOffset)->getRecordRuntimeScore() );
			this->nextItemsFromChildren.at(childOffset)->getRecordMatchingPrefixes(recordKeywordMatchPrefixes);
			this->nextItemsFromChildren.at(childOffset)->getRecordMatchEditDistances(recordKeywordMatchEditDistances);
			this->nextItemsFromChildren.at(childOffset)->getRecordMatchAttributeBitmaps(matchedAttributeIdsList);
			this->nextItemsFromChildren.at(childOffset)->getPositionIndexOffsets(positionIndexOffsets);
			this->nextItemsFromChildren.at(childOffset)->getTermTypes(termTypes);
			// static score, not for now
			// and remove it from this list by substituting it by the next one
			this->nextItemsFromChildren.at(childOffset) =
					this->getPhysicalPlanOptimizationNode()->getChildAt(childOffset)->getExecutableNode()->getNext(params);
		}
	}
	// prepare the record and return it
	minIDRecord->setRecordRuntimeScore(params.ranker->computeAggregatedRuntimeScoreForOr(runtimeScores));
	minIDRecord->setRecordMatchingPrefixes(recordKeywordMatchPrefixes);
	minIDRecord->setRecordMatchEditDistances(recordKeywordMatchEditDistances);
	minIDRecord->setRecordMatchAttributeBitmaps(matchedAttributeIdsList);
	minIDRecord->setPositionIndexOffsets(positionIndexOffsets);
	minIDRecord->setTermTypes(termTypes);

	this->visitedRecords.push_back(minIDRecord->getRecordId());

	return minIDRecord;
}
bool UnionSortedByIDOperator::close(PhysicalPlanExecutionParameters & params){
	// close children
	for(unsigned childOffset = 0 ; childOffset < this->getPhysicalPlanOptimizationNode()->getChildrenCount() ; ++childOffset){
		this->getPhysicalPlanOptimizationNode()->getChildAt(childOffset)->getExecutableNode()->close(params);
	}
	this->listsHaveMoreRecordsInThem = true;
	visitedRecords.clear();
	return true;
}

string UnionSortedByIDOperator::toString(){
	string result = "UnionSortedByIDOperator" ;
	if(this->getPhysicalPlanOptimizationNode()->getLogicalPlanNode() != NULL){
		result += this->getPhysicalPlanOptimizationNode()->getLogicalPlanNode()->toString();
	}
	return result;
}

bool UnionSortedByIDOperator::verifyByRandomAccess(PhysicalPlanRandomAccessVerificationParameters & parameters) {
	return verifyByRandomAccessOrHelper(this->getPhysicalPlanOptimizationNode(), parameters);
}

// The cost of open of a child is considered only once in the cost computation
// of parent open function.
PhysicalPlanCost UnionSortedByIDOptimizationOperator::getCostOfOpen(const PhysicalPlanExecutionParameters & params){
	PhysicalPlanCost resultCost;
	// cost of opening children
	for(unsigned childOffset = 0 ; childOffset != this->getChildrenCount() ; ++childOffset){
		resultCost = resultCost + this->getChildAt(childOffset)->getCostOfOpen(params);
	}

	// cost of initializing nextItems vector
	for(unsigned childOffset = 0 ; childOffset != this->getChildrenCount() ; ++childOffset){
		resultCost = resultCost + this->getChildAt(childOffset)->getCostOfGetNext(params);
	}

	return resultCost;
}
// The cost of getNext of a child is multiplied by the estimated number of calls to this function
// when the cost of parent is being calculated.
PhysicalPlanCost UnionSortedByIDOptimizationOperator::getCostOfGetNext(const PhysicalPlanExecutionParameters & params) {
	/*
	 * cost : sum of size of all children / estimatedNumberOfResults
	 */
	double sumOfAllCosts = 0;
	for(unsigned childOffset = 0 ; childOffset != this->getChildrenCount() ; ++childOffset){
		sumOfAllCosts += this->getChildAt(childOffset)->getLogicalPlanNode()->stats->getEstimatedNumberOfResults() *
				this->getChildAt(childOffset)->getCostOfGetNext(params).cost;
	}
	unsigned estimatedNumberOfResults = this->getLogicalPlanNode()->stats->getEstimatedNumberOfResults();

	PhysicalPlanCost resultCost;
	resultCost = ( (sumOfAllCosts*1.0) / estimatedNumberOfResults ) + 1;
	return resultCost;

}
// the cost of close of a child is only considered once since each node's close function is only called once.
PhysicalPlanCost UnionSortedByIDOptimizationOperator::getCostOfClose(const PhysicalPlanExecutionParameters & params) {
	PhysicalPlanCost resultCost;

	// cost of closing children
	for(unsigned childOffset = 0 ; childOffset != this->getChildrenCount() ; ++childOffset){
		resultCost = resultCost + this->getChildAt(childOffset)->getCostOfClose(params);
	}

	return resultCost;
}
PhysicalPlanCost UnionSortedByIDOptimizationOperator::getCostOfVerifyByRandomAccess(const PhysicalPlanExecutionParameters & params){
	PhysicalPlanCost resultCost;

	// cost of verifying children
	for(unsigned childOffset = 0 ; childOffset != this->getChildrenCount() ; ++childOffset){
		resultCost = resultCost + this->getChildAt(childOffset)->getCostOfVerifyByRandomAccess(params);
	}

	return resultCost;
}

void UnionSortedByIDOptimizationOperator::getOutputProperties(IteratorProperties & prop){
	prop.addProperty(PhysicalPlanIteratorProperty_SortById);
}
void UnionSortedByIDOptimizationOperator::getRequiredInputProperties(IteratorProperties & prop){
	// the only requirement for input is to be sorted by ID
	prop.addProperty(PhysicalPlanIteratorProperty_SortById);
}
PhysicalPlanNodeType UnionSortedByIDOptimizationOperator::getType() {
	return PhysicalPlanNode_UnionSortedById;
}
bool UnionSortedByIDOptimizationOperator::validateChildren(){
	for(unsigned i = 0 ; i < getChildrenCount() ; i++){
		PhysicalPlanOptimizationNode * child = getChildAt(i);
		PhysicalPlanNodeType childType = child->getType();
		switch (childType) {
			case PhysicalPlanNode_RandomAccessTerm:
			case PhysicalPlanNode_RandomAccessAnd:
			case PhysicalPlanNode_RandomAccessOr:
			case PhysicalPlanNode_RandomAccessNot:
			case PhysicalPlanNode_RandomAccessGeo:
			case PhysicalPlanNode_UnionLowestLevelTermVirtualList:
			case PhysicalPlanNode_GeoNearestNeighbor:
				// this operator cannot have TVL as a child, TVL overhead is not needed for this operator
				return false;
			default:{
				continue;
			}
		}
	}
	return true;
}

}
}
