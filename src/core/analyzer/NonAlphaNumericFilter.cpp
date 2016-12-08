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
 * NonAlphaNumericFilter.cpp
 *
 *  Created on: Nov 13, 2013
 *      Author: sbisht
 */

#include "NonAlphaNumericFilter.h"

namespace srch2 {
namespace instantsearch {

NonAlphaNumericFilter::NonAlphaNumericFilter(TokenStream *tokenStream,
                                             const ProtectedWordsContainer *_protectedWordsContainer) :
    TokenFilter(tokenStream),
    protectedWordsContainer(_protectedWordsContainer)
{
    this->tokenStreamContainer = tokenStream->tokenStreamContainer;
}

void NonAlphaNumericFilter::clearState() {
    // clear the state of the filter in the upstream
    if (this->tokenStream != NULL)
        this->tokenStream->clearState();
  
    // clear our own states
    while (!this->internalTokenBuffer.empty())
        this->internalTokenBuffer.pop();
}

/*
 *    This filter receives tokens from upstream filter/tokenizer and then further tokenizes
 *    them based on delimiters. It is advised to have this filter right after the tokenizer.
 *
 *    1. If the token is a protected token, then we do not analyze it further. Instead, we just
 *       send it downstream
 *    2. If the token is not a protected token then we try to tokenize it based on the delimiters
 *       and store the tokens in an internal buffer. This buffer should be flushed to downstream
 *       filters first before asking for new tokens from the upstream tokenizer.
 *
 *    input tokens = { I love c++ and java-script programming }
 *    output tokens = {  I love c++ and java-script programming }  , c++ is a protected keyword
 *    but java-script is not a protected keyword.
 */
bool NonAlphaNumericFilter::processToken()
{
	while(1) {
		// if we have tokens in the filter's internal buffer then flush them out one by one before
		// requesting a new token from the upstream.
		if (internalTokenBuffer.size() == 0) {
			if (this->tokenStream != NULL && !this->tokenStream->processToken()) {
				return false;
			}
			string currentToken;
			charTypeVectorToUtf8String(this->tokenStreamContainer->currentToken, currentToken);

			if (isProtectWord(currentToken)) {
				return true;      // Do not apply any filter on protected keywords such as C++
			}

			unsigned currOffset = 0;
			unsigned offsetOfToken = 0;
			unsigned origOffset = this->tokenStreamContainer->currentTokenOffset;
			const vector<CharType> & charTypeBuffer = this->tokenStreamContainer->currentToken;
			vector<CharType> tempToken;
			// Try to tokenize keywords based on delimiters
			while (currOffset < charTypeBuffer.size()) {
				const CharType& c = charTypeBuffer[currOffset];
				switch (characterSet.getCharacterType(c)) {
				case CharSet::DELIMITER_TYPE:
				case CharSet::WHITESPACE:
					if (!tempToken.empty()) {
						internalTokenBuffer.push(make_pair(tempToken, origOffset + offsetOfToken));
						tempToken.clear();
					}
					offsetOfToken = currOffset + 1;
					break;
				default:
					tempToken.push_back(c);
					break;
				}
				currOffset++;
			}
			if (!tempToken.empty()) {  // whatever is left over, push it to the internal buffer.
				internalTokenBuffer.push(make_pair(tempToken, origOffset + offsetOfToken));
				tempToken.clear();
			}
			if (internalTokenBuffer.size() > 0) {
				// put first element from the internal token buffer to a shared token buffer for other
				// filters to consume.  e.g internal buffer = {java script}, put "java" to
				// the shared token.
				this->tokenStreamContainer->currentToken = internalTokenBuffer.front().first;
				this->tokenStreamContainer->currentTokenOffset = internalTokenBuffer.front().second;
				internalTokenBuffer.pop();
				return true;
			}
		} else {
			this->tokenStreamContainer->currentToken = internalTokenBuffer.front().first;
			this->tokenStreamContainer->currentTokenPosition++;
			this->tokenStreamContainer->currentTokenOffset = internalTokenBuffer.front().second;
			internalTokenBuffer.pop();
			return true;
		}
	}
	return false; // to avoid compiler warning
}


NonAlphaNumericFilter::~NonAlphaNumericFilter() {
}

} /* namespace instanstsearch */
} /* namespace srch2 */
