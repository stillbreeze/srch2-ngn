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
 * GeoNearestNeighborOperator.cpp
 *
 *  Created on: Jul 11, 2014
 */
#include "GeoNearestNeighborOperator.h"
#include "PhysicalOperatorsHelper.h"
#include "src/core/util/Logger.h"
#include "src/core/util/RecordSerializerUtil.h"
#include "src/core/util/RecordSerializer.h"

using srch2::util::Logger;

namespace srch2 {
namespace instantsearch {

GeoNearestNeighborOperator::GeoNearestNeighborOperator(){
	this->queryEvaluator = NULL;
	this->queryShape = NULL;
}

GeoNearestNeighborOperator::~GeoNearestNeighborOperator(){
	for(unsigned i = 0 ; i < this->heapItems.size() ; i++){
		delete this->heapItems[i];
	}
	this->heapItems.clear();
}

bool GeoNearestNeighborOperator::open(QueryEvaluatorInternal * queryEvaluator, PhysicalPlanExecutionParameters & params){
	this->queryEvaluator = queryEvaluator;
	// get the forward list read view
	this->forwardListDirectoryReadView = this->queryEvaluator->indexReadToken.forwardIndexReadViewSharedPtr;
	// finding the query region
	this->queryShape = this->getPhysicalPlanOptimizationNode()->getLogicalPlanNode()->regionShape;
	// get quadTreeNodeSet which contains all the subtrees in quadtree which have the answers
	this->quadTreeNodeSetSharedPtr = this->getPhysicalPlanOptimizationNode()->getLogicalPlanNode()->stats->getQuadTreeNodeSetForEstimation();
	vector<QuadTreeNode*>* quadTreeNodeSet = this->quadTreeNodeSetSharedPtr->getQuadTreeNodeSet();
	// put all the QuadTree nodes from quadTreeNodeSet into the heap vector

	for( unsigned i = 0 ; i < quadTreeNodeSet->size() ; i++ ){
		this->heapItems.push_back(new GeoNearestNeighborOperatorHeapItem(quadTreeNodeSet->at(i),this->queryShape));
	}
	// make the heap
	make_heap(this->heapItems.begin(),this->heapItems.end(),GeoNearestNeighborOperator::GeoNearestNeighborOperatorHeapItemCmp());

	// finding the offset of the latitude and longitude attribute in the refining attributes' memory
	// we put this part in open function because we don't want to repeat it for each record
	// base on our experiments this part of the code takes more time. So it is more sufficient to use it here and save
	// the offset of latitude and longitude in the class.
	getLat_Long_Offset(this->latOffset, this->longOffset, queryEvaluator->getSchema());

	return true;
}

PhysicalPlanRecordItem* GeoNearestNeighborOperator::getNext(const PhysicalPlanExecutionParameters & params){
	GeoNearestNeighborOperatorHeapItem* heapItem;
	// Iterate on heap until find a geoElement on top of the heap
	while(this->heapItems.size() > 0){
		// pop the first item of the heap
		heapItem = this->heapItems.front();
		pop_heap(heapItems.begin(),heapItems.end(), GeoNearestNeighborOperator::GeoNearestNeighborOperatorHeapItemCmp());
		heapItems.pop_back();

		if(heapItem->isQuadTreeNode){ // the top of the heap is a quadtree node.
			if(heapItem->quadtreeNode->getIsLeaf()){ // this node is leaf. So we should insert all its elements to the heap.
				vector<GeoElement*>* elements = heapItem->quadtreeNode->getElements();
				for( unsigned i = 0 ; i < elements->size() ; i++ ){
					if(queryShape->contain(elements->at(i)->point)){
						heapItems.push_back(new GeoNearestNeighborOperatorHeapItem(elements->at(i), queryShape));
						// push the new item into the heap
						push_heap(heapItems.begin(),
								  heapItems.end(),
								  GeoNearestNeighborOperator::GeoNearestNeighborOperatorHeapItemCmp());
					}
				}
			}else{ // this node is an internal node. So we should insert all its children to the heap.
				vector<QuadTreeNode*>* children = heapItem->quadtreeNode->getChildren();
				for( unsigned i = 0 ; i < children->size() ; i++ ){
					if(children->at(i) != NULL){
						heapItems.push_back(new GeoNearestNeighborOperatorHeapItem(children->at(i), queryShape));
						// push the new item into the heap
						push_heap(heapItems.begin(),
								  heapItems.end(),
								  GeoNearestNeighborOperator::GeoNearestNeighborOperatorHeapItemCmp());
					}
				}
			}
		}else{ // the top of the heap is a geoElement. So we can return it if it is in the query region
			// check the record and return it if it's valid.
			bool valid = false;
			const ForwardList* forwardList = this->queryEvaluator->indexReadToken.getForwardList(heapItem->geoElement->forwardListID, valid);
			if(valid && this->queryShape->contain(heapItem->geoElement->point)){
				PhysicalPlanRecordItem* newItem = this->queryEvaluator->getPhysicalPlanRecordItemPool()->createRecordItem();
				newItem->setIsGeo(true); // this Item is for a geo element
				// record id
				newItem->setRecordId(heapItem->geoElement->forwardListID);
				// runtime score
				newItem->setRecordRuntimeScore(heapItem->geoElement->getScore(*this->queryShape));
				delete heapItem;
				return newItem;
			}
		}
		delete heapItem;
	}
	return NULL;
}

bool GeoNearestNeighborOperator::close(PhysicalPlanExecutionParameters & params){
	this->queryEvaluator = NULL;
	for( unsigned i = 0 ; i < this->heapItems.size() ; i++ ){
		delete this->heapItems[i];
	}
	this->heapItems.clear();
	this->queryShape = NULL;
	return true;
}

bool GeoNearestNeighborOperator::verifyByRandomAccess(PhysicalPlanRandomAccessVerificationParameters & parameters){
	return verifyByRandomAccessGeoHelper(parameters, this->queryEvaluator, this->queryShape, this->latOffset, this->longOffset);
}

string GeoNearestNeighborOperator::toString(){
	string result = "GeoNearestNeighborOperator";
	if(this->getPhysicalPlanOptimizationNode()->getLogicalPlanNode() != NULL){
		result += this->getPhysicalPlanOptimizationNode()->getLogicalPlanNode()->toString();
	}
	return result;
}
// The cost of open of a child is considered only once in the cost computation
// of parent open function.
PhysicalPlanCost GeoNearestNeighborOptimizationOperator::getCostOfOpen(const PhysicalPlanExecutionParameters & params){
	PhysicalPlanCost resultCost;
	resultCost.cost = this->getLogicalPlanNode()->stats->quadTreeNodeSet->sizeOfQuadTreeNodeSet();
	return resultCost;
}
// The cost of getNext of a child is multiplied by the estimated number of calls to this function
// when the cost of parent is being calculated.
PhysicalPlanCost GeoNearestNeighborOptimizationOperator::getCostOfGetNext(const PhysicalPlanExecutionParameters & params) {
	PhysicalPlanCost resultCost;
	unsigned estimatedNumberOfleafNodes = this->getLogicalPlanNode()->stats->estimatedNumberOfLeafNodes;
	// cost of removing the item from the heap
	// TODO: consider the number of all geoelements in the heap
	double cost = 0;
	resultCost.cost = log2(((double)(this->getLogicalPlanNode()->stats->quadTreeNodeSet->sizeOfQuadTreeNodeSet() + GEO_MAX_NUM_OF_ELEMENTS + 1)));
	return resultCost;
}
// the cost of close of a child is only considered once since each node's close function is only called once.
PhysicalPlanCost GeoNearestNeighborOptimizationOperator::getCostOfClose(const PhysicalPlanExecutionParameters & params) {
	PhysicalPlanCost resultCost;
	unsigned estimatedNumberOfleafNodes = this->getLogicalPlanNode()->stats->estimatedNumberOfLeafNodes;
	resultCost.cost =  this->getLogicalPlanNode()->stats->quadTreeNodeSet->sizeOfQuadTreeNodeSet() + GEO_MAX_NUM_OF_ELEMENTS;
	return resultCost;
}

PhysicalPlanCost GeoNearestNeighborOptimizationOperator::getCostOfVerifyByRandomAccess(const PhysicalPlanExecutionParameters & params){
	// we only need to check if the query region contains the record's point or not
	PhysicalPlanCost resultCost;
	resultCost.cost = 1;
	return resultCost;
}

void GeoNearestNeighborOptimizationOperator::getOutputProperties(IteratorProperties & prop){
	prop.addProperty(PhysicalPlanIteratorProperty_SortByScore);
}

void GeoNearestNeighborOptimizationOperator::getRequiredInputProperties(IteratorProperties & prop){
	prop.addProperty(PhysicalPlanIteratorProperty_LowestLevel);
}

PhysicalPlanNodeType GeoNearestNeighborOptimizationOperator::getType() {
	return PhysicalPlanNode_GeoNearestNeighbor;
}

bool GeoNearestNeighborOptimizationOperator::validateChildren(){
	// this operator cannot have any children
	if(getChildrenCount() > 0){
		return false;
	}
	return true;
}

}
}



