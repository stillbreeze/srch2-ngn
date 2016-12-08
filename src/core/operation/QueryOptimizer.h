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

#ifndef __WRAPPER_QUERYOPTIMIZER_H__
#define __WRAPPER_QUERYOPTIMIZER_H__

#include "instantsearch/Constants.h"
#include "operation/HistogramManager.h"
#include "physical_plan/PhysicalPlan.h"
#include "instantsearch/LogicalPlan.h"

using namespace std;

namespace srch2 {
namespace instantsearch {

class QueryEvaluatorInternal;
/*
 * This class is responsible of doing two tasks :
 * 1. Translating LogicalPlan and building PhysicalPlan from it
 * 2. Applying rules on PhysicalPlan and optimizing it
 *
 * The physical plan built by this class is then executed to procduce the result set.
 */
class QueryOptimizer {
public:
	QueryOptimizer(QueryEvaluatorInternal * queryEvaluator);

	/*
	 * 1. Find the search type based on searchType coming from wrapper layer and
	 * ---- post processing needs.
	 * 2. Prepare the ranker object in and put it in PhysicalPlan. (Note that because of
	 * ---- heuristics which will be chosen later, searchType and ranker might change in later steps)
	 *  // TODO : write other steps.
	 *
	 * 2. this function builds PhysicalPlan and optimizes it
	 * ---- 2.1. Builds the Physical plan by mapping each Logical operator to a/multiple Physical operator(s)
	 *           and makes sure inputs and outputs of operators are consistent.
	 * ---- 2.2. Applies optimization rules on the physical plan
	 */
	void buildAndOptimizePhysicalPlan(PhysicalPlan & physicalPlan, LogicalPlan * logicalPlan, unsigned planOffset);

private:

	/*
	 * This function maps LogicalPlan nodes to Physical nodes and builds a very first
	 * version of the PhysicalPlan. This plan will optimizer in next steps.
	 */
	void buildPhysicalPlanFirstVersion(PhysicalPlan & physicalPlan, unsigned planOffset);

	void chooseSearchTypeOfPhysicalPlan(PhysicalPlan & physicalPlan);

	void buildIncompleteTreeOptions(vector<PhysicalPlanOptimizationNode *> & treeOptions);

	void buildIncompleteSubTreeOptions(LogicalPlanNode * root, vector<PhysicalPlanOptimizationNode *> & treeOptions);
	void buildIncompleteSubTreeOptionsAndOr(LogicalPlanNode * root, vector<PhysicalPlanOptimizationNode *> & treeOptions);
	void buildIncompleteSubTreeOptionsNot(LogicalPlanNode * root, vector<PhysicalPlanOptimizationNode *> & treeOptions);
	void buildIncompleteSubTreeOptionsTerm(LogicalPlanNode * root, vector<PhysicalPlanOptimizationNode *> & treeOptions);
	void buildIncompleteSubTreeOptionsPhrase(LogicalPlanNode * root, vector<PhysicalPlanOptimizationNode *> & treeOptions);
	void buildIncompleteSubTreeOptionsGeo(LogicalPlanNode * root, vector<PhysicalPlanOptimizationNode *> & treeOptions);
	void injectRequiredSortOperators(vector<PhysicalPlanOptimizationNode *> & treeOptions);
	void injectRequiredSortOperators(PhysicalPlanOptimizationNode * root);

	void injectSortOperatorsForFeedback(PhysicalPlanOptimizationNode **root,
			PhysicalPlanOptimizationNode *node, unsigned childOffset);
	bool isNodeFeedbackCapable(PhysicalPlanOptimizationNode *node);
	bool isLogicalPlanBoosted(PhysicalPlanOptimizationNode *node);

	PhysicalPlanOptimizationNode * findTheMinimumCostTree(vector<PhysicalPlanOptimizationNode *> & treeOptions, PhysicalPlan & physicalPlan, unsigned planOffset);

	PhysicalPlanNode * buildPhysicalPlanFirstVersionFromTreeStructure(PhysicalPlanOptimizationNode * chosenTree);
	/*
	 * this function applies optimization rules (funtions starting with Rule_) on the plan one by one
	 */
	void applyOptimizationRulesOnThePlan(PhysicalPlan & physicalPlan);

	/*
	 * An example of an optimization rule function.
	 */
	void Rule_1(PhysicalPlan & physicalPlan);

	QueryEvaluatorInternal * queryEvaluator;
	LogicalPlan * logicalPlan;
};

}
}

#endif //__WRAPPER_QUERYOPTIMIZER_H__
