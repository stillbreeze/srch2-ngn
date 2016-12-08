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

#include <instantsearch/Term.h>

#include <map>
#include <vector>
#include <string>
#include <stdint.h>
#include "util/encoding.h"
#include <sstream>

using std::string;
using std::vector;
namespace srch2
{
namespace instantsearch
{

//TODO OPT pass string pointer in FuzzyTerm/ExactTerm constructor rather than copying. But copy terms into cache
struct Term::Impl
{
    string keyword;
    TermType type;
    float boost;
    float similarityBoost;
    uint8_t threshold;
    vector<unsigned> searchableAttributeIdsToFilter;
    ATTRIBUTES_OP attrOp;

    string toString(){
    	std::stringstream ss;
    	ss << keyword.c_str();
    	ss << type;
    	ss << boost;
    	ss << similarityBoost;
    	ss << (threshold+1) << "";
    	ss << attrOp;
    	for (unsigned i = 0; i < searchableAttributeIdsToFilter.size(); ++i)
    		ss << searchableAttributeIdsToFilter[i];
    	return ss.str();
    }
};

Term::Term(const string &keywordStr, TermType type, const float boost, const float fuzzyMatchPenalty, const uint8_t threshold)
{
    impl = new Impl;
    impl->keyword = keywordStr;
    impl->type = type;
    impl->boost = boost;
    impl->similarityBoost = fuzzyMatchPenalty;
    impl->threshold = threshold;
    impl->attrOp = ATTRIBUTES_OP_OR;
}

Term::~Term()
{
    if (impl != NULL) {
        delete impl;
    }
}

float Term::getBoost() const
{
    return impl->boost;
}

void Term::setBoost(const float boost)
{
    if (boost >= 1 && boost <= 2)
    {
        impl->boost = boost;
    }
    else
    {
        impl->boost = 1;
    }
}

float Term::getSimilarityBoost() const
{
    return impl->similarityBoost;
}

void Term::setSimilarityBoost(const float similarityBoost)
{
    if (similarityBoost >= 0.0 && similarityBoost <= 1.0)
    {
        impl->similarityBoost = similarityBoost;
    }
    else
    {
        impl->similarityBoost = 0.5;
    }
}

void Term::setThreshold(const uint8_t threshold)
{
    switch (threshold)
    {
        case 0: case 1: case 2:    case 3:    case 4:    case 5:
            impl->threshold = threshold;
            break;
        default:
            impl->threshold = 0;
    };
}

uint8_t Term::getThreshold() const
{
    return impl->threshold;
}

string *Term::getKeyword()
{
    return &(impl->keyword);
}

string *Term::getKeyword() const
{
    return &(impl->keyword);
}

TermType Term::getTermType() const
{
    return impl->type;
}

void Term::addAttributesToFilter(const vector<unsigned>& searchableAttributeId, ATTRIBUTES_OP attrOp)
{
    this->impl->searchableAttributeIdsToFilter = searchableAttributeId;
    this->impl->attrOp = attrOp;
}

ATTRIBUTES_OP Term::getFilterAttrOperation() {
	return this->impl->attrOp;
}

vector<unsigned>& Term::getAttributesToFilter() const
{
    return this->impl->searchableAttributeIdsToFilter;
}

string Term::toString(){
	return this->impl->toString();
}

////////////////////////

Term* ExactTerm::create(const string &keyword, TermType type, const float boost, const float similarityBoost)
{
    return new Term(keyword, type, boost, similarityBoost, 0);
}

Term* FuzzyTerm::create(const string &keyword, TermType type, const float boost, const float similarityBoost, const uint8_t threshold)
{
    return new Term(keyword, type, boost, similarityBoost, threshold);
}

}}

