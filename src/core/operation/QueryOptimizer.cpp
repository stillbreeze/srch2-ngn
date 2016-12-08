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
#include "QueryOptimizer.h"
#include "util/QueryOptimizerUtil.h"
#include "physical_plan/PhysicalOperators.h"
#include "QueryEvaluatorInternal.h"
#include "physical_plan/FilterQueryOperator.h"
#include "util/Logger.h"
#include  "physical_plan/FeedbackRankingOperator.h"
namespace srch2 {
namespace instantsearch {

QueryOptimizer::QueryOptimizer(QueryEvaluatorInternal * queryEvaluator){
    this->queryEvaluator = queryEvaluator;
}

/*
 *
 * this function builds PhysicalPlan and optimizes it
 * ---- 1. Builds the Physical plan by mapping each Logical operator to a/multiple Physical operator(s)
 *           and makes sure inputs and outputs of operators are consistent.
 * ---- 2. Applies optimization rules on the physical plan
 */
void QueryOptimizer::buildAndOptimizePhysicalPlan(PhysicalPlan & physicalPlan,LogicalPlan * logicalPlan, unsigned planOffset){

    this->logicalPlan = logicalPlan;

    // Build physical plan
    buildPhysicalPlanFirstVersion(physicalPlan, planOffset);

    if(physicalPlan.getPlanTree() == NULL){
        return;
    }

    // apply optimization rules
    applyOptimizationRulesOnThePlan(physicalPlan);

}


/*
 * This function maps LogicalPlan nodes to Physical nodes and builds a very first
 * version of the PhysicalPlan. This plan will optimizer in next steps.
 */
void QueryOptimizer::buildPhysicalPlanFirstVersion(PhysicalPlan & physicalPlan, unsigned planOffset){

    //1. Choose the search type based on user's request and post processing
    chooseSearchTypeOfPhysicalPlan(physicalPlan);


    //3. Build all the options for physical plan tree
    vector<PhysicalPlanOptimizationNode *> treeOptions;
    buildIncompleteTreeOptions(treeOptions);

    //4. Find the option which has the minimum cost
    PhysicalPlanOptimizationNode * chosenTree = findTheMinimumCostTree(treeOptions,physicalPlan,planOffset);

    if(chosenTree == NULL){
        physicalPlan.setPlanTree(NULL);
        return;
    }

    // 5. Build the complete version of physical plan tree from the chosen tree structure
    // --- and set in the plan object
    physicalPlan.setPlanTree(buildPhysicalPlanFirstVersionFromTreeStructure(chosenTree));

    // print for test
//    Logger::info("========================================================");
//    physicalPlan.getPlanTree()->getPhysicalPlanOptimizationNode()->printSubTree();
//    Logger::info("========================================================");
//    exit(0);
//    cout << "========================================================" << endl;
//    physicalPlan.getPlanTree()->getPhysicalPlanOptimizationNode()->printSubTree();
//    cout << "========================================================" << endl;
    // end : print for test

}

/*
 * Decides on the searchType of the physical plan. this searchType might change later because of
 * optimization heuristics.
 */
void QueryOptimizer::chooseSearchTypeOfPhysicalPlan(PhysicalPlan & physicalPlan){
    // TODO : for now, we just simply pass the same search type
    physicalPlan.setSearchType(logicalPlan->getQueryType());
}

void QueryOptimizer::buildIncompleteTreeOptions(vector<PhysicalPlanOptimizationNode *> & treeOptions){

    buildIncompleteSubTreeOptions(logicalPlan->getTree() , treeOptions);

    // some options are ALL random-access , we should remove them
    vector<PhysicalPlanOptimizationNode *> treeOptionsCopy = treeOptions;
    treeOptions.clear();
    for(vector<PhysicalPlanOptimizationNode *>::iterator treeOption = treeOptionsCopy.begin() ; treeOption != treeOptionsCopy.end() ; ++treeOption){
        switch ((*treeOption)->getType()) {
            case PhysicalPlanNode_RandomAccessTerm:
            case PhysicalPlanNode_RandomAccessAnd:
            case PhysicalPlanNode_RandomAccessOr:
            case PhysicalPlanNode_RandomAccessNot:
            case PhysicalPlanNode_RandomAccessGeo:
                // delete
                break;
            default:{
                treeOptions.push_back(*treeOption);
                break;
            }
        }
    }

    // now we should inject SORT operators to make these plans functional
    injectRequiredSortOperators(treeOptions);

    if(logicalPlan->getPostProcessingInfo() == NULL ||
    		logicalPlan->getPostProcessingInfo()->getSortEvaluator() == NULL){
    	// If external sort criteria is not specified by the user, only then we will do feedback boosting.
    	// e.g if sort-by-refining-attribute is present in query then ignore feedback boosting.
    	for(vector<PhysicalPlanOptimizationNode *>::iterator treeOption = treeOptions.begin();
    			treeOption != treeOptions.end(); ++treeOption){
    		if (!isLogicalPlanBoosted(*treeOption))
    			injectSortOperatorsForFeedback(&(*treeOption), *treeOption,  -1 /*dummy*/);
    	}
    }
//    // print for test
//    cout << "Number of initial plans : " << treeOptions.size() << endl;
//    cout << "========================================================" << endl;
//    for(vector<PhysicalPlanOptimizationNode *>::iterator treeOption = treeOptions.begin() ; treeOption != treeOptions.end() ; ++treeOption){
//        (*treeOption)->printSubTree();
//        cout << "========================================================" << endl;
//    }
//    exit(0);
//    // end : print for test
}

void QueryOptimizer::buildIncompleteSubTreeOptions(LogicalPlanNode * root, vector<PhysicalPlanOptimizationNode *> & treeOptions){
    if(root == NULL) return;
    switch (root->nodeType) {
        case LogicalPlanNodeTypeAnd:
        case LogicalPlanNodeTypeOr:
            buildIncompleteSubTreeOptionsAndOr(root, treeOptions);
            break;
        case LogicalPlanNodeTypeNot:
            buildIncompleteSubTreeOptionsNot(root, treeOptions);
            break;
        case LogicalPlanNodeTypePhrase:
            buildIncompleteSubTreeOptionsPhrase(root, treeOptions);
            break;
        case LogicalPlanNodeTypeTerm:
            buildIncompleteSubTreeOptionsTerm(root, treeOptions);
            break;
        case LogicalPlanNodeTypeGeo:
        	buildIncompleteSubTreeOptionsGeo(root, treeOptions);
        	break;
    }

}

void QueryOptimizer::buildIncompleteSubTreeOptionsAndOr(LogicalPlanNode * root, vector<PhysicalPlanOptimizationNode *> & treeOptions){
    vector<vector<PhysicalPlanOptimizationNode *> > candidatesOfChildren;
    unsigned * domains = new unsigned[root->children.size()];
    unsigned totalNumberOfProducts = 1;
    unsigned childIndex = 0;
    for(vector<LogicalPlanNode *>::const_iterator child = root->children.begin() ; child != root->children.end(); ++child){
        vector<PhysicalPlanOptimizationNode *> childTreeOptions;
        buildIncompleteSubTreeOptions(*child, childTreeOptions);
        candidatesOfChildren.push_back(childTreeOptions);
        domains[childIndex] = childTreeOptions.size();
        totalNumberOfProducts *= childTreeOptions.size();
        //
        childIndex ++;
    }

    if(totalNumberOfProducts > 500){
        totalNumberOfProducts = 500;
    }

    // now we must find all possible combinations of children options
    unsigned * cartProductResults = new unsigned[totalNumberOfProducts * root->children.size()];
    srch2::util::QueryOptimizerUtil::cartesianProduct(root->children.size(), domains, cartProductResults, totalNumberOfProducts);



    // now we should build one tree for each cartesian product and for each AND option
    // move on combinations of children options
    for(unsigned p = 0 ; p < totalNumberOfProducts ; ++p){
        // add AND implementation options that we have
        vector<PhysicalPlanOptimizationNode *> ourOptions;
        if(root->nodeType == LogicalPlanNodeTypeAnd){
            ourOptions.push_back((PhysicalPlanOptimizationNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createMergeTopKOptimizationOperator());
            ourOptions.push_back((PhysicalPlanOptimizationNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createMergeByShortestListOptimizationOperator());
            ourOptions.push_back((PhysicalPlanOptimizationNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createMergeSortedByIDOptimizationOperator());
            ourOptions.push_back((PhysicalPlanOptimizationNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createRandomAccessVerificationAndOptimizationOperator());
        }else if(root->nodeType == LogicalPlanNodeTypeOr){
            ourOptions.push_back((PhysicalPlanOptimizationNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createUnionSortedByIDOptimizationOperator());
            ourOptions.push_back((PhysicalPlanOptimizationNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createRandomAccessVerificationOrOptimizationOperator());
        }else{
            ASSERT(false);
        }

        // move on our options and set the logical plan node
        for(vector<PhysicalPlanOptimizationNode *>::iterator ourOption = ourOptions.begin(); ourOption != ourOptions.end() ; ++ourOption){
            (*ourOption)->setLogicalPlanNode(root);
        }

        // move on our options
        for(vector<PhysicalPlanOptimizationNode *>::iterator ourOption = ourOptions.begin(); ourOption != ourOptions.end() ; ++ourOption){
            // move on children
            for(unsigned d = 0; d < root->children.size() ; d++){
                vector<PhysicalPlanOptimizationNode *> * candidatesOfThisChild = &(candidatesOfChildren.at(d));
                unsigned indexOfChildOptionToChoose = cartProductResults[root->children.size() * p + d];
                (*ourOption)->addChild(candidatesOfThisChild->at(indexOfChildOptionToChoose));
            }
            if((*ourOption)->validateChildren() == true){
                treeOptions.push_back(*ourOption);
            }
//            else{
//                    cout << "========================== INVALID PLAN==============================" << endl;
//                    (*ourOption)->printSubTree();
//                    cout << "========================================================" << endl;
//            }
        }
    }

    delete[] domains;
    delete[] cartProductResults;

}

void QueryOptimizer::buildIncompleteSubTreeOptionsPhrase(LogicalPlanNode * root,
                                            vector<PhysicalPlanOptimizationNode *> & treeOptions) {

    vector<PhysicalPlanOptimizationNode *> childTreeOptions;
    buildIncompleteSubTreeOptions(root->children.at(0), childTreeOptions);

    for(unsigned p = 0 ; p < childTreeOptions.size(); ++p) {
        PhysicalPlanOptimizationNode * phraseSearchOptNode = (PhysicalPlanOptimizationNode *)
                this->queryEvaluator->getPhysicalOperatorFactory()->createPhraseSearchOptimzationOperator();
        phraseSearchOptNode->setLogicalPlanNode(root);
        phraseSearchOptNode->addChild(childTreeOptions[p]);
        if(phraseSearchOptNode->validateChildren() == true) {
            treeOptions.push_back(phraseSearchOptNode);
        }
    }
}

void QueryOptimizer::buildIncompleteSubTreeOptionsNot(LogicalPlanNode * root, vector<PhysicalPlanOptimizationNode *> & treeOptions){
    // TODO For now NOT just passes the options up
    // NOTE : This is WRONG because it's ignoring NOT
     vector<PhysicalPlanOptimizationNode *> childrenOptions;
    buildIncompleteSubTreeOptions(root->children.at(0), childrenOptions);

    for(unsigned optionOffset = 0; optionOffset < childrenOptions.size() ; ++optionOffset){
        PhysicalPlanOptimizationNode * newNotObj =(PhysicalPlanOptimizationNode *)
                this->queryEvaluator->getPhysicalOperatorFactory()->createRandomAccessVerificationNotOptimizationOperator();
        newNotObj->setLogicalPlanNode(root);
        newNotObj->addChild(childrenOptions.at(optionOffset));
        treeOptions.push_back(newNotObj);
    }
}
void QueryOptimizer::buildIncompleteSubTreeOptionsTerm(LogicalPlanNode * root, vector<PhysicalPlanOptimizationNode *> & treeOptions){
    if(root->forcedPhysicalNode == PhysicalPlanNode_NOT_SPECIFIED){
        PhysicalPlanOptimizationNode * op = (PhysicalPlanOptimizationNode *)
                this->queryEvaluator->getPhysicalOperatorFactory()->createUnionLowestLevelTermVirtualListOptimizationOperator();
        op->setLogicalPlanNode(root);
        treeOptions.push_back(op);
        op = (PhysicalPlanOptimizationNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createRandomAccessVerificationTermOptimizationOperator();
        op->setLogicalPlanNode(root);
        treeOptions.push_back(op);
        op = (PhysicalPlanOptimizationNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createUnionLowestLevelSimpleScanOptimizationOperator();
        op->setLogicalPlanNode(root);
        treeOptions.push_back(op);
    }else if(root->forcedPhysicalNode == PhysicalPlanNode_UnionLowestLevelSuggestion){
        PhysicalPlanOptimizationNode * op = (PhysicalPlanOptimizationNode *)
                this->queryEvaluator->getPhysicalOperatorFactory()->createUnionLowestLevelSuggestionOptimizationOperator();
        op->setLogicalPlanNode(root);
        treeOptions.push_back(op);
    }
}

// if the type is not specified we will create all the options for this node
// and if the type is specified in the forcedPhysicalNode variable then we only create that type
void QueryOptimizer::buildIncompleteSubTreeOptionsGeo(LogicalPlanNode * root, vector<PhysicalPlanOptimizationNode *> & treeOptions){
	if(root->forcedPhysicalNode == PhysicalPlanNode_NOT_SPECIFIED){
		PhysicalPlanOptimizationNode *op = (PhysicalPlanOptimizationNode *)
					this->queryEvaluator->getPhysicalOperatorFactory()->createGeoNearestNeighborOptimizationOperator();
		op->setLogicalPlanNode(root);
		treeOptions.push_back(op);
		op = (PhysicalPlanOptimizationNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createRandomAccessVerificationGeoOptimizationOperator();
		op->setLogicalPlanNode(root);
		treeOptions.push_back(op);
		op = (PhysicalPlanOptimizationNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createGeoSimpleScanOptimizationOperator();
		op->setLogicalPlanNode(root);
		treeOptions.push_back(op);
	}else if(root->forcedPhysicalNode == PhysicalPlanNode_RandomAccessGeo){
		PhysicalPlanOptimizationNode *op = (PhysicalPlanOptimizationNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createRandomAccessVerificationGeoOptimizationOperator();
		op->setLogicalPlanNode(root);
		treeOptions.push_back(op);
	}
}

/*
 *  Determine whether the logical plan is feedback boosted.
 */
bool QueryOptimizer::isLogicalPlanBoosted(PhysicalPlanOptimizationNode *node) {

	if (isNodeFeedbackCapable(node)){
		node->setFeedbackBoosted();
		return true;
	}

	if (node->getChildrenCount() == 0) {
		// nodes are marked not boosted by default.
		return false;
	}

	bool isAllChildBoosted = true;
	for (unsigned i = 0; i < node->getChildrenCount(); ++i) {
		if (!isLogicalPlanBoosted(node->getChildAt(i))) {
			isAllChildBoosted = false;
			break;
		}
	}

	if (isAllChildBoosted) {
		node->setFeedbackBoosted();
		return true;
	} else {
		return false;
	}
}

void QueryOptimizer::injectSortOperatorsForFeedback(PhysicalPlanOptimizationNode **root,
		PhysicalPlanOptimizationNode *node, unsigned childOffset){

	bool atLeastOneChildBoosted = false;
	for(unsigned i = 0 ; i < node->getChildrenCount() ; ++i){
		if (node->getChildAt(i)->isFeedbackBoosted()) {
			atLeastOneChildBoosted = true;
			break;
		}
	}

	if (atLeastOneChildBoosted) {
		// There are some children which are boosted and some are not boosted.
		// traverse to the non-boosted children and fix them by inserting sort operator..
		for(unsigned i = 0 ; i < node->getChildrenCount() ; ++i) {
			if (node->getChildAt(i)->isFeedbackBoosted() == false) {
				injectSortOperatorsForFeedback(root, node->getChildAt(i), i);
			}
		}
	} else {
		// ALL nodes are NOT boosted or there are no children.
		// fix this node by injecting a sortByScore operator here
		SortByScoreOptimizationOperator * sortByScoreOp =
				this->queryEvaluator->getPhysicalOperatorFactory()->createSortByScoreOptimizationOperator();
		sortByScoreOp->setLogicalPlanNode(node->getLogicalPlanNode());
		sortByScoreOp->addChild(node);

		PhysicalPlanOptimizationNode* parentNode = node->getParent();
		if (parentNode != NULL) {
			parentNode->setChildAt(childOffset, sortByScoreOp);
		} else {
			// must be root node. make sort-by-score new root.
			(*root) = sortByScoreOp;
		}
	}
}

// Operator is feedback capable iff
// 1. it does NOT have sorted-by-score output property.
// OR
// 2. it is either MergeTopK or SortByScore operators. ( These operators have feedback logic in it)
//
bool QueryOptimizer::isNodeFeedbackCapable(PhysicalPlanOptimizationNode *node) {

	IteratorProperties nodeOutputProp;
	node->getOutputProperties(nodeOutputProp);

	bool hasSortByScoreProperty = false;
	for (unsigned i = 0; i < nodeOutputProp.properties.size(); ++i) {
		if (nodeOutputProp.properties[i] == PhysicalPlanIteratorProperty_SortByScore) {
			hasSortByScoreProperty = true;
			break;
		}
	}
	if (!hasSortByScoreProperty || node->getType() == PhysicalPlanNode_SortByScore
			|| node->getType() == PhysicalPlanNode_MergeTopK) {
		return true;
	} else {
		return false;
	}
}

void QueryOptimizer::injectRequiredSortOperators(vector<PhysicalPlanOptimizationNode *> & treeOptions){
    // 1. iterate on trees and inject sort operators
    for(vector<PhysicalPlanOptimizationNode *>::iterator treeOption = treeOptions.begin();
            treeOption != treeOptions.end(); ++treeOption){
        injectRequiredSortOperators(*treeOption);
        // 2. make sure the output order is by score, if not, add a sortByScore operator
        IteratorProperties finalRequiredOrder;
        finalRequiredOrder.addProperty(PhysicalPlanIteratorProperty_SortByScore);
        IteratorProperties finalTreeProperties;
        (*treeOption)->getOutputProperties(finalTreeProperties);
        IteratorProperties reason;
        if( finalTreeProperties.isMatchAsInputTo(finalRequiredOrder, reason) == false  ){
            // we must inject a sortByScore operator here
            SortByScoreOptimizationOperator * sortByScoreOp =
                    this->queryEvaluator->getPhysicalOperatorFactory()->createSortByScoreOptimizationOperator();
            sortByScoreOp->setLogicalPlanNode((*treeOption)->getLogicalPlanNode());
            sortByScoreOp->addChild(*treeOption);
            *treeOption = sortByScoreOp;
        }
    }
}

void QueryOptimizer::injectRequiredSortOperators(PhysicalPlanOptimizationNode * root){
    // 1. first move on children and call this function for all subtrees
    for(unsigned i = 0 ; i < root->getChildrenCount() ; ++i){
        PhysicalPlanOptimizationNode * child = root->getChildAt(i);
        injectRequiredSortOperators(child);
    }
    // 2. inject sort operators between root and its children if needed
    for(unsigned i = 0 ; i < root->getChildrenCount() ; ++i){
        PhysicalPlanOptimizationNode * child = root->getChildAt(i);

        // 0. if child is a Varification child no sort operator should be inserted
        bool shouldCheckProperties = true;
        switch (child->getType()) {
            case PhysicalPlanNode_RandomAccessTerm:
            case PhysicalPlanNode_RandomAccessAnd:
            case PhysicalPlanNode_RandomAccessOr:
            case PhysicalPlanNode_RandomAccessNot:
            case PhysicalPlanNode_RandomAccessGeo:
                shouldCheckProperties = false;
                break;
            default:
                break;
        }
        if(! shouldCheckProperties){
            continue;
        }
        // 1. first check if this child's output properties are compatible with root's input properties
        IteratorProperties childOutputProp;
        child->getOutputProperties(childOutputProp);
        IteratorProperties rootInputProp;
        root->getRequiredInputProperties(rootInputProp);
        IteratorProperties reasonOfDisMatch;
        if ( childOutputProp.isMatchAsInputTo(rootInputProp,reasonOfDisMatch)  == true){
            continue; // this child is a good child, no need to inject anything
        }else{
            for(vector<PhysicalPlanIteratorProperty>::iterator property = reasonOfDisMatch.properties.begin();
                    property != reasonOfDisMatch.properties.end() ; ++property){
                switch (*property) {
                    case PhysicalPlanIteratorProperty_SortById:
                    {
                        // we must inject a SortById operator here
                        SortByIdOptimizationOperator * sortByIdOp =
                                this->queryEvaluator->getPhysicalOperatorFactory()->createSortByIdOptimizationOperator();
                        sortByIdOp->setLogicalPlanNode(child->getLogicalPlanNode());
                        sortByIdOp->addChild(child);
                        root->setChildAt(i, sortByIdOp);
                        break;
                    }
                    case PhysicalPlanIteratorProperty_SortByScore:
                    {
                        // we must inject a sortByScore operator here
                        SortByScoreOptimizationOperator * sortByScoreOp =
                                this->queryEvaluator->getPhysicalOperatorFactory()->createSortByScoreOptimizationOperator();
                        sortByScoreOp->setLogicalPlanNode(child->getLogicalPlanNode());
                        sortByScoreOp->addChild(child);
                        root->setChildAt(i, sortByScoreOp);
                        break;
                    }
                    default:
                    {
                        //ASSERT(false);
                        break;
                    }
                }
            }
        }

    }
}

PhysicalPlanOptimizationNode * QueryOptimizer::findTheMinimumCostTree(vector<PhysicalPlanOptimizationNode *> & treeOptions, PhysicalPlan & physicalPlan, unsigned planOffset){

    PhysicalPlanOptimizationNode * minPlan = NULL;
    unsigned minCost = 0;


    if(treeOptions.size() == 1){
        return treeOptions.at(0);
    }

    unsigned treeOptionIndexChosen = 0 ;
    for(unsigned treeOptionIndex = 0 ; treeOptionIndex + 1 < treeOptions.size() ; treeOptionIndex++){
        PhysicalPlanOptimizationNode * treeOption = treeOptions.at(treeOptionIndex);
        PhysicalPlanCost cost;
        unsigned numberOfGetNextCalls = physicalPlan.getExecutionParameters()->k;
        if(treeOption->getLogicalPlanNode()->stats->getEstimatedNumberOfResults() < numberOfGetNextCalls){
            numberOfGetNextCalls = treeOption->getLogicalPlanNode()->stats->getEstimatedNumberOfResults();
        }


        cost = cost + treeOption->getCostOfOpen(*(physicalPlan.getExecutionParameters()));
        cost = cost +
                treeOption->getCostOfGetNext(*(physicalPlan.getExecutionParameters())).cost *
                numberOfGetNextCalls;
        cost = cost + treeOption->getCostOfClose(*(physicalPlan.getExecutionParameters()));


        if(minPlan == NULL){
            minPlan = treeOption;
            minCost = cost.cost;
            treeOptionIndexChosen = treeOptionIndex;
        }else{
            if(minCost > cost.cost){
                minPlan = treeOption;
                minCost = cost.cost;
                treeOptionIndexChosen = treeOptionIndex;
            }
        }
    }
    return minPlan;
}

PhysicalPlanNode * QueryOptimizer::buildPhysicalPlanFirstVersionFromTreeStructure(PhysicalPlanOptimizationNode * chosenTree){
    // now we move on the tree structure and create the real physical plan
    PhysicalPlanOptimizationNode * optimizationResult = NULL;
    PhysicalPlanNode * executableResult = NULL;
    // TODO possible optimization : we can use PhysicalPlanOptimizationNodes from the chosenTree
    // allocate filter query operator to be attached to a term (if this filter exists)
    FilterQueryOperator * filterQueryOp = NULL;
    if(logicalPlan->getPostProcessingInfo() != NULL &&
        (chosenTree->getType() == 
         PhysicalPlanNode_UnionLowestLevelTermVirtualList || 
         chosenTree->getType() == 
         PhysicalPlanNode_UnionLowestLevelSimpleScanOperator
         || chosenTree->getType() == 
         PhysicalPlanNode_UnionLowestLevelSuggestion
         || chosenTree->getType() ==
         PhysicalPlanNode_GeoNearestNeighbor
         || chosenTree->getType() ==
         PhysicalPlanNode_GeoSimpleScan)){
    	// we need to create a FilterQueryOperator if we have a filter in the query or if we have record-base access control.
        if(logicalPlan->getPostProcessingInfo()->getFilterQueryEvaluator() != NULL  || logicalPlan->getPostProcessingInfo()->getRoleId()->compare("") != 0){
            filterQueryOp = this->queryEvaluator->getPhysicalOperatorFactory()->
                    createFilterQueryOperator(logicalPlan->getPostProcessingInfo()->getFilterQueryEvaluator(),
                    		*(logicalPlan->getPostProcessingInfo()->getRoleId()));
            FilterQueryOptimizationOperator * filterQueryOpOp = this->queryEvaluator->getPhysicalOperatorFactory()->
                    createFilterQueryOptimizationOperator();
            filterQueryOp->setPhysicalPlanOptimizationNode(filterQueryOpOp);
            filterQueryOpOp->setExecutableNode(filterQueryOp);
        }
    }
    switch (chosenTree->getType()) {
        case PhysicalPlanNode_SortById:{
            optimizationResult = (PhysicalPlanOptimizationNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createSortByIdOptimizationOperator();
            executableResult = (PhysicalPlanNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createSortByIdOperator();
            break;
        }
        case PhysicalPlanNode_SortByScore:{
            optimizationResult = (PhysicalPlanOptimizationNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createSortByScoreOptimizationOperator();
            executableResult = (PhysicalPlanNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createSortByScoreOperator();
            break;
        }
        case PhysicalPlanNode_MergeTopK:{
            optimizationResult = (PhysicalPlanOptimizationNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createMergeTopKOptimizationOperator();
            executableResult = (PhysicalPlanNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createMergeTopKOperator();
            break;
        }
        case PhysicalPlanNode_MergeSortedById:{
            optimizationResult = (PhysicalPlanOptimizationNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createMergeSortedByIDOptimizationOperator();
            executableResult = (PhysicalPlanNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createMergeSortedByIDOperator();
            break;
        }
        case PhysicalPlanNode_MergeByShortestList:{
            optimizationResult = (PhysicalPlanOptimizationNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createMergeByShortestListOptimizationOperator();
            executableResult = (PhysicalPlanNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createMergeByShortestListOperator();
            break;
        }
        case PhysicalPlanNode_UnionSortedById:{
            optimizationResult = (PhysicalPlanOptimizationNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createUnionSortedByIDOptimizationOperator();
            executableResult = (PhysicalPlanNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createUnionSortedByIDOperator();
            break;
        }
        case PhysicalPlanNode_UnionLowestLevelTermVirtualList:{
            optimizationResult = (PhysicalPlanOptimizationNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createUnionLowestLevelTermVirtualListOptimizationOperator();
            executableResult = (PhysicalPlanNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createUnionLowestLevelTermVirtualListOperator();
            break;
        }
        case PhysicalPlanNode_UnionLowestLevelSimpleScanOperator:{
            optimizationResult = (PhysicalPlanOptimizationNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createUnionLowestLevelSimpleScanOptimizationOperator();
            executableResult = (PhysicalPlanNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createUnionLowestLevelSimpleScanOperator();
            break;
        }
        case PhysicalPlanNode_GeoNearestNeighbor:{
        	optimizationResult = (PhysicalPlanOptimizationNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createGeoNearestNeighborOptimizationOperator();
        	executableResult = (PhysicalPlanNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createGeoNearestNeighborOperator();
        	break;
        }
        case PhysicalPlanNode_GeoSimpleScan:{
        	optimizationResult = (PhysicalPlanOptimizationNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createGeoSimpleScanOptimizationOperator();
        	executableResult = (PhysicalPlanNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createGeoSimpleScanOperator();
        	break;
        }
        case PhysicalPlanNode_RandomAccessGeo:{
        	optimizationResult = (PhysicalPlanOptimizationNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createRandomAccessVerificationGeoOptimizationOperator();
        	executableResult = (PhysicalPlanNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createRandomAccessVerificationGeoOperator();
        	break;
        }
        case PhysicalPlanNode_RandomAccessTerm:{
            optimizationResult = (PhysicalPlanOptimizationNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createRandomAccessVerificationTermOptimizationOperator();
            executableResult = (PhysicalPlanNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createRandomAccessVerificationTermOperator();
            break;
        }
        case PhysicalPlanNode_RandomAccessAnd:{
            optimizationResult = (PhysicalPlanOptimizationNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createRandomAccessVerificationAndOptimizationOperator();
            executableResult = (PhysicalPlanNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createRandomAccessVerificationAndOperator();
            break;
        }
        case PhysicalPlanNode_RandomAccessOr:{
            optimizationResult = (PhysicalPlanOptimizationNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createRandomAccessVerificationOrOptimizationOperator();
            executableResult = (PhysicalPlanNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createRandomAccessVerificationOrOperator();
            break;
        }
        case PhysicalPlanNode_RandomAccessNot:{
            optimizationResult = (PhysicalPlanOptimizationNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createRandomAccessVerificationNotOptimizationOperator();
            executableResult = (PhysicalPlanNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createRandomAccessVerificationNotOperator();
            break;
        }
        case PhysicalPlanNode_UnionLowestLevelSuggestion:{
            optimizationResult = (PhysicalPlanOptimizationNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createUnionLowestLevelSuggestionOptimizationOperator();
            executableResult = (PhysicalPlanNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createUnionLowestLevelSuggestionOperator();
            break;
        }
        case PhysicalPlanNode_PhraseSearch:{
            optimizationResult = (PhysicalPlanOptimizationNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createPhraseSearchOptimzationOperator();
            LogicalPlanNode * logPlanNode = chosenTree->getLogicalPlanNode();
            ASSERT(logPlanNode != NULL);    //TODO: throw exception ...phrase opertator cannot work without phrase info stored in logical node.
            LogicalPlanPhraseNode * phraseLogicalNode = reinterpret_cast<LogicalPlanPhraseNode *>(logPlanNode);
            executableResult = (PhysicalPlanNode *)this->queryEvaluator->getPhysicalOperatorFactory()->createPhraseSearchOperator(phraseLogicalNode->getPhraseInfo());
            break;
        }
        default:
            ASSERT(false);
            break;
    }
    optimizationResult->setLogicalPlanNode(chosenTree->getLogicalPlanNode());
    optimizationResult->setExecutableNode(executableResult);
    executableResult->setPhysicalPlanOptimizationNode(optimizationResult);




    for(unsigned childOffset = 0 ; childOffset < chosenTree->getChildrenCount() ; ++childOffset){
        PhysicalPlanOptimizationNode * child = chosenTree->getChildAt(childOffset);
        optimizationResult->addChild(buildPhysicalPlanFirstVersionFromTreeStructure(child)->getPhysicalPlanOptimizationNode());
    }

    if(filterQueryOp != NULL){
        filterQueryOp->getPhysicalPlanOptimizationNode()->addChild(optimizationResult);
        filterQueryOp->getPhysicalPlanOptimizationNode()->setLogicalPlanNode(optimizationResult->getLogicalPlanNode());
        executableResult = (PhysicalPlanNode *)filterQueryOp;
    }

    return executableResult;
}


/*
 * this function applies optimization rules (funtions starting with Rule_) on the plan one by one
 */
void QueryOptimizer::applyOptimizationRulesOnThePlan(PhysicalPlan & physicalPlan){
    //TODO
    // calls the optimization rules one by one
    Rule_1(physicalPlan);
    //Rule_2(physicalPlan);
    //Rule_3(physicalPlan);
    //...
}

/*
 * An example of an optimization rule function.
 */
void QueryOptimizer::Rule_1(PhysicalPlan & physicalPlan){
    //TODO
}


}
}
