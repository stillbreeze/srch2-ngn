/*
 * SharedToken.h
 *
 *  Created on: 2013-5-17
 */

#ifndef __CORE_ANALYZER_TOKENSTREAMCONTAINER_H__
#define __CORE_ANALYZER_TOKENSTREAMCONTAINER_H__

#include <vector>
#include "util/encoding.h"
#include "CharSet.h"

namespace srch2
{
namespace instantsearch
{

class TokenStreamContainer {
public:
	TokenStreamContainer():offset(0), currentTokenPosition(0){}
    void fillInCharacters(const std::vector<CharType> &charVector, bool isPrefix=false){
        currentToken.clear();
        completeCharVector = charVector;
        currentTokenPosition = 0;
        offset = 0;
        this->isPrefix = isPrefix;
    }
	/*
	 * For example:  process "We went to school"
	 * 		completeCharVector = "We went to school"
	 * 		When we process to the first character 'W', currentToken="W", offset=0
	 * 		When we move to the second character 'e', currentToken="We", offset=1
	 */
	std::vector<CharType> currentToken;			//current token
	std::vector<CharType> completeCharVector; 	//complete char vector of a string
	int offset;									//the offset of current position to process
	unsigned currentTokenPosition;
	bool isPrefix;
};
}}
#endif /* __CORE_ANALYZER__TOKENSTREAMCONTAINER_H__ */
