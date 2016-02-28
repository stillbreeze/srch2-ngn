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
#include "ChineseAnalyzer.h"
#include "ChineseTokenizer.h"
#include "LowerCaseFilter.h"
#include "StopFilter.h"
#include "SynonymFilter.h"
#include "util/Logger.h"

namespace srch2{
namespace instantsearch{

ChineseAnalyzer::ChineseAnalyzer(const ChineseDictionaryContainer* chineseDictionaryContainer,
                                 const StopWordContainer *stopWords,
                                 const ProtectedWordsContainer *protectedWords,
                                 const SynonymContainer *synonyms,
                                 const std::string &delimiters)
    : AnalyzerInternal(NULL/*stemmer*/, stopWords, protectedWords, synonyms, delimiters),
      mChineseDictionaryContainer(chineseDictionaryContainer)
{
	this->tokenStream = NULL;
    this->analyzerType = CHINESE_ANALYZER;
}

AnalyzerType ChineseAnalyzer::getAnalyzerType() const
{
    return CHINESE_ANALYZER;
}

TokenStream* ChineseAnalyzer::createOperatorFlow(){
    TokenStream *tokenStream = new ChineseTokenizer(mChineseDictionaryContainer);
    tokenStream = new LowerCaseFilter(tokenStream);

    if (this->stopWords != NULL) {
        tokenStream = new StopFilter(tokenStream, this->stopWords);
    }

    if (this->synonyms != NULL) {
        tokenStream = new SynonymFilter(tokenStream, this->synonyms);
    }

    return tokenStream;
}

}
}
