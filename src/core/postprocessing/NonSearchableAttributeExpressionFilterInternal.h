//$Id: ResultsPostProcessor.h 3456 2013-06-26 02:11:13Z Jamshid $

/*
 * The Software is made available solely for use according to the License Agreement. Any reproduction
 * or redistribution of the Software not in accordance with the License Agreement is expressly prohibited
 * by law, and may result in severe civil and criminal penalties. Violators will be prosecuted to the
 * maximum extent possible.
 *
 * THE SOFTWARE IS WARRANTED, IF AT ALL, ONLY ACCORDING TO THE TERMS OF THE LICENSE AGREEMENT. EXCEPT
 * AS WARRANTED IN THE LICENSE AGREEMENT, SRCH2 INC. HEREBY DISCLAIMS ALL WARRANTIES AND CONDITIONS WITH
 * REGARD TO THE SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES AND CONDITIONS OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT.  IN NO EVENT SHALL SRCH2 INC. BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF SOFTWARE.

 * Copyright © 2010 SRCH2 Inc. All rights reserved
 */


#ifndef _CORE_POSTPROCESSING_NONSEARCHABLEATTRIBUTEFILTERINTERNAL_H_
#define _CORE_POSTPROCESSING_NONSEARCHABLEATTRIBUTEFILTERINTERNAL_H_

#include "instantsearch/Schema.h"
#include "index/ForwardIndex.h"
#include "instantsearch/Score.h"
#include "query/QueryResultsInternal.h"
#include "string"
#include <vector>
using namespace std;


namespace srch2
{
namespace instantsearch
{


class NonSearchableAttributeExpressionFilterInternal
{


public:

	NonSearchableAttributeExpressionFilterInternal(NonSearchableAttributeExpressionFilter * filter){
		this->filter = filter;
	}

	// evaluates expression object coming from query using result data to see
	// if it passes the query criterion.
	bool doesPassCriterion(Schema * schema, ForwardIndex * forwardIndex , const QueryResult * result){


		// fetch the names and ids of non searchable attributes from schema
		vector<string> attributes;
		vector<unsigned> attributeIds;
		for(map<string,unsigned>::const_iterator attr = schema->getNonSearchableAttributes()->begin();
				attr != schema->getNonSearchableAttributes()->end() ; ++attr ){
			attributes.push_back(attr->first);
			attributeIds.push_back(attr->second);
		}

		// now fetch the values of different attributes from forward index
		vector<Score> scores;
		bool isValid = false;
		const ForwardList * list = forwardIndex->getForwardList(result->internalRecordId , isValid);
		ASSERT(isValid);
		const VariableLengthAttributeContainer * container = list->getNonSearchableAttributeContainer();
		container->getBatchOfAttributes(attributeIds,schema,&scores);

		// now call the evaluator to see it this record passes the criteria or not
		map<string, Score> valuesForEvaluation;

		// prepare the evaluator input
		unsigned s =0;
		for(vector<string>::iterator attr = attributes.begin() ; attr != attributes.end() ; ++attr ){
			valuesForEvaluation[*attr] = scores.at(s);
			s++;
		}

		return filter->evaluator->evaluate(valuesForEvaluation);

	}

private:
	NonSearchableAttributeExpressionFilter * filter ;

};


}
}
#endif // _CORE_POSTPROCESSING_NONSEARCHABLEATTRIBUTEFILTERINTERNAL_H_