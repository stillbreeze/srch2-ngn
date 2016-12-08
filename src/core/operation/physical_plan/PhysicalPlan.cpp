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
#include "PhysicalPlan.h"
#include "operation/QueryEvaluatorInternal.h"
#include "util/QueryOptimizerUtil.h"

using namespace std;

namespace srch2 {
namespace instantsearch {



// This function is called to determine whether the output properties of an operator
// match the input properties of another one.
bool IteratorProperties::isMatchAsInputTo(const IteratorProperties & prop , IteratorProperties & reason){
	bool result = true;
	for(vector<PhysicalPlanIteratorProperty>::const_iterator property = prop.properties.begin();
			property != prop.properties.end() ; ++property){
		if(find(this->properties.begin(),this->properties.end(), *property) == this->properties.end()){
			reason.addProperty(*property);
			result = false;
		}
	}

	return result;
}

// Adds this property to the list of properties that this object has
void IteratorProperties::addProperty(PhysicalPlanIteratorProperty prop){
	this->properties.push_back(prop);
}



void PhysicalPlanOptimizationNode::setChildAt(unsigned offset, PhysicalPlanOptimizationNode * child) {
	if(offset >= children.size()){
		ASSERT(false);
		return;
	}
	children.at(offset) = child;
}

void PhysicalPlanOptimizationNode::addChild(PhysicalPlanOptimizationNode * child) {
	if(child == NULL){
		ASSERT(false);
		return;
	}
	children.push_back(child);
}


// parent is not set in the code, it's going to be removed if not needed TODO
void PhysicalPlanOptimizationNode::setParent(PhysicalPlanOptimizationNode * parent){
	this->parent = parent;
}

PhysicalPlanOptimizationNode * PhysicalPlanOptimizationNode::getParent(){
	return parent;
}

void PhysicalPlanOptimizationNode::printSubTree(unsigned indent){
	PhysicalPlanNodeType type = getType();
	switch (type) {
		case PhysicalPlanNode_SortById:
			Logger::info("[SortByID]");
			break;
		case PhysicalPlanNode_SortByScore:
			Logger::info("[SortByScore]");
			break;
		case PhysicalPlanNode_MergeTopK:
			Logger::info("[AND TopK]");
			break;
		case PhysicalPlanNode_MergeSortedById:
			Logger::info("[AND SortedByID]");
			break;
		case PhysicalPlanNode_MergeByShortestList:
			Logger::info("[AND ShortestList]");
			break;
		case PhysicalPlanNode_UnionSortedById:
			Logger::info("[OR SortedByID]");
			break;
		case PhysicalPlanNode_UnionLowestLevelTermVirtualList:
			Logger::info("[TVL]");
			break;
		case PhysicalPlanNode_UnionLowestLevelSimpleScanOperator:
			Logger::info("[SCAN]");
			break;
		case PhysicalPlanNode_GeoNearestNeighbor:
			Logger::info("[GEO NearestNeighbor]");
			break;
		case PhysicalPlanNode_GeoSimpleScan:
			Logger::info("[GEO SimpleScan]");
			break;
		case PhysicalPlanNode_RandomAccessGeo:
			Logger::info("[R.A.GEO]");
			break;
		case PhysicalPlanNode_RandomAccessTerm:
			Logger::info("[TERM]" );
			break;
		case PhysicalPlanNode_RandomAccessAnd:
			Logger::info("[R.A.AND]");
			break;
		case PhysicalPlanNode_RandomAccessOr:
			Logger::info("[R.A.OR]");
			break;
		case PhysicalPlanNode_RandomAccessNot:
			Logger::info("[R.A.NOT]");
			break;
		default:
			Logger::info("Somthing else");
	}
	for(vector<PhysicalPlanOptimizationNode *>::iterator child = children.begin(); child != children.end() ; ++child){
		(*child)->printSubTree(indent+1);
	}
}

void PhysicalPlanNode::setPhysicalPlanOptimizationNode(PhysicalPlanOptimizationNode * optimizationNode){
	this->optimizationNode = optimizationNode;
}

PhysicalPlan::PhysicalPlan(	QueryEvaluatorInternal * queryEvaluator){
	this->queryEvaluator = 	queryEvaluator;
	this->tree = NULL;
	this->executionParameters = NULL;
}

PhysicalPlan::~PhysicalPlan(){
//	if(tree != NULL) delete tree;
//	if(executionParameters != NULL) delete executionParameters;
}



PhysicalPlanNode * PhysicalPlan::getPlanTree(){
	return this->tree;
}

void PhysicalPlan::setPlanTree(PhysicalPlanNode * tree){
	this->tree = tree;
}

void PhysicalPlan::setSearchType(srch2is::QueryType searchType){
	this->searchType = searchType;
}

srch2is::QueryType PhysicalPlan::getSearchType(){
	return this->searchType;
}

void PhysicalPlan::setExecutionParameters(PhysicalPlanExecutionParameters * executionParameters){
	ASSERT(this->executionParameters == NULL && executionParameters != NULL);
	this->executionParameters = executionParameters;
}

PhysicalPlanExecutionParameters * PhysicalPlan::getExecutionParameters(){
	ASSERT(this->executionParameters != NULL);
	return this->executionParameters;
}


/*
 * The implementor of this function is supposed to append a unique string to 'uniqueString'
 * which determines the subtree of physical plan uniquely.
 * For example,
 * if the physical plan is :
 * [MergeTopK]______[TVL for terminator]
 *        |
 *        |_________[TVL for movie]
 *        |
 *        |_________[TVL for trailer]
 *
 * A call to this function with ignoreLastLeafNode = true will ignore the last leaf node and
 * give us a string like this :
 * MergeTopK_TVL_terminator_TVL_movie
 * NOTE: this string is much more complicated than that because it basically
 * serialized all the objects in physical plan. You can look at toString functions
 * to understand what's prepared finally.
 */
void PhysicalPlanNode::getUniqueStringForCache(bool ignoreLastLeafNode, string & uniqueString){
	// get number of children
	unsigned numberOfChildren = this->getPhysicalPlanOptimizationNode()->getChildrenCount();

	// if we don't have any child,
	if(numberOfChildren == 0){
		if(ignoreLastLeafNode){ // if ignoreLastLeafNode is true we should ignore self
			return;
		}else{
			uniqueString += toString();
		}
	}else{ // if there are some children, ignoreLastLeafNode can only be true for the last one
		uniqueString += toString();
		for(unsigned childOffset = 0 ; childOffset < numberOfChildren - 1 ; childOffset ++){
			this->getPhysicalPlanOptimizationNode()->getChildAt(childOffset)->
					getExecutableNode()->getUniqueStringForCache(false , uniqueString);
		}
		this->getPhysicalPlanOptimizationNode()->getChildAt(numberOfChildren - 1)->
				getExecutableNode()->getUniqueStringForCache(ignoreLastLeafNode , uniqueString);
	}
}

}}
