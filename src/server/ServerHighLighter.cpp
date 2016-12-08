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
 * ServerHighLighter.cpp
 *
 *  Created on: Jan 27, 2014
 */

#include "ServerHighLighter.h"
#include "Srch2Server.h"
#include "AnalyzerFactory.h"
#include <instantsearch/QueryResults.h>
#include "ParsedParameterContainer.h"
#include "util/RecordSerializer.h"
#include "query/QueryResultsInternal.h"
#include "util/RecordSerializerUtil.h"

using namespace srch2::util;
using namespace srch2::instantsearch;

namespace srch2 {
namespace httpwrapper {
/*
 *   The function loops over all the query results and calls genSnippetsForSingleRecord for
 *   each query result.
 */
void ServerHighLighter::generateSnippets(vector<RecordSnippet>& highlightInfo){

	unsigned upperLimit = queryResults->getNumberOfResults();
	if (upperLimit > HighlightRecOffset + HighlightRecCount)
		upperLimit = HighlightRecOffset + HighlightRecCount;
	unsigned lowerLimit = HighlightRecOffset;
	for (unsigned i = lowerLimit; i < upperLimit; ++i) {
		RecordSnippet recordSnippets;
		unsigned recordId = queryResults->getInternalRecordId(i);
		genSnippetsForSingleRecord(queryResults, i, recordSnippets);
		recordSnippets.recordId =recordId;
		highlightInfo.push_back(recordSnippets);
	}
}

void buildKeywordHighlightInfo(const QueryResults * qr, unsigned recIdx,
		vector<keywordHighlightInfo>& keywordStrToHighlight);
void buildKeywordHighlightInfo(const QueryResults * qr, unsigned recIdx,
		vector<keywordHighlightInfo>& keywordStrToHighlight){
	vector<string> matchingKeywords;
	qr->getMatchingKeywords(recIdx, matchingKeywords);
	vector<TermType> termTypes;
	qr->getTermTypes(recIdx, termTypes);
	vector<unsigned> editDistances;
	qr->getEditDistances(recIdx, editDistances);
	vector<vector<unsigned> > matchedAttributeIdsList;
	qr->getMatchedAttributes(recIdx, matchedAttributeIdsList);
	for (unsigned i = 0; i <  matchingKeywords.size(); ++i) {
		keywordHighlightInfo keywordInfo;
		if(termTypes.at(i) == TERM_TYPE_COMPLETE)
			keywordInfo.flag = HIGHLIGHT_KEYWORD_IS_COMPLETE;
		else if (termTypes.at(i) == TERM_TYPE_PHRASE)
			keywordInfo.flag = HIGHLIGHT_KEYWORD_IS_PHRASE;
		else
			keywordInfo.flag = HIGHLIGHT_KEYWORD_IS_PERFIX;
		utf8StringToCharTypeVector(matchingKeywords[i], keywordInfo.key);
		keywordInfo.editDistance = editDistances.at(i);
		keywordInfo.attributeIdsList = matchedAttributeIdsList.at(i);
		keywordStrToHighlight.push_back(keywordInfo);
	}
}
/*
 *   The function generates snippet for all the highlight attributes of a given query result.
 *   Attribute values are fetched from a compact representation stored in forward index.
 */
void ServerHighLighter::genSnippetsForSingleRecord(const QueryResults *qr, unsigned recIdx, RecordSnippet& recordSnippets) {

		/*
		 *  Code below is a setup for highlighter module
		 */
		unsigned recordId = qr->getInternalRecordId(recIdx);

		vector<keywordHighlightInfo> keywordStrToHighlight;
		buildKeywordHighlightInfo(qr, recIdx, keywordStrToHighlight);

        StoredRecordBuffer buffer =  server->indexer->getInMemoryData(recordId);
        if (buffer.start.get() == NULL)
        	return;
        const vector<std::pair<unsigned, string> >&highlightAttributes = server->indexDataConfig->getHighlightAttributeIdsVector();
        for (unsigned i = 0 ; i < highlightAttributes.size(); ++i) {
    		AttributeSnippet attrSnippet;
        	unsigned id = highlightAttributes[i].first;
        	// check whether the searchable attribute is accessible for current role-id.
        	// snippet is generated for accessible searchable attributes only.
        	bool isFieldAccessible = server->indexer->getAttributeAcl().isSearchableFieldAccessibleForRole(
        			aclRoleValue, highlightAttributes[i].second);
        	if (!isFieldAccessible)
        		continue;  // ignore unaccessible attributes. Do not generate snippet.
        	unsigned lenOffset = compactRecDeserializer->getSearchableOffset(id);
        	const char *attrdata = buffer.start.get() + *((unsigned *)(buffer.start.get() + lenOffset));
        	unsigned len = *(((unsigned *)(buffer.start.get() + lenOffset)) + 1) -
        			*((unsigned *)(buffer.start.get() + lenOffset));
        	snappy::Uncompress(attrdata,len, &uncompressedInMemoryRecordString);
        	try{
				this->highlightAlgorithms->getSnippet(qr, recIdx, highlightAttributes[i].first,
						uncompressedInMemoryRecordString, attrSnippet.snippet,
						storedAttrSchema->isSearchableAttributeMultiValued(id), keywordStrToHighlight);
        	}catch(const exception& ex) {
        		Logger::debug("could not generate a snippet for an record/attr %d/%d", recordId, id);
        	}
        	attrSnippet.FieldId = highlightAttributes[i].second;
        	if (attrSnippet.snippet.size() > 0)
        		recordSnippets.fieldSnippets.push_back(attrSnippet);
        }
        if (recordSnippets.fieldSnippets.size() == 0)
        	Logger::warn("could not generate a snippet because search keywords could not be found in any attribute of record!!");
}

ServerHighLighter::ServerHighLighter(QueryResults * queryResults,Srch2Server *server,
		ParsedParameterContainer& param, unsigned offset, unsigned count) {

	this->queryResults = queryResults;

	HighlightConfig hconf;
	string pre, post;
	server->indexDataConfig->getExactHighLightMarkerPre(pre);
	server->indexDataConfig->getExactHighLightMarkerPost(post);
	hconf.highlightMarkers.push_back(make_pair(pre, post));
	server->indexDataConfig->getFuzzyHighLightMarkerPre(pre);
	server->indexDataConfig->getFuzzyHighLightMarkerPost(post);
	hconf.highlightMarkers.push_back(make_pair(pre, post));
	server->indexDataConfig->getHighLightSnippetSize(hconf.snippetSize);
	this->aclRoleValue = param.roleId;
	if (!isEnabledWordPositionIndex(server->indexer->getSchema()->getPositionIndexType())){
		// we do not need phrase information because position index is not enabled.
		param.PhraseKeyWordsInfoMap.clear();
	}
	/*
	 *  We have two ways of generating snippets.
	 *  1. Using term offsets stored in the forward index.
	 *  OR
	 *  2. By generating offset information at runtime using analyzer.
	 *
	 *  If isEnabledCharPositionIndex returns true then the term offset information is available
	 *  in the forward index. In that case we use the term offset based logic.
	 */
	// Note: check for server schema not the configuration.
	if (isEnabledCharPositionIndex(server->indexer->getSchema()->getPositionIndexType())) {
		this->highlightAlgorithms  = new TermOffsetAlgorithm(server->indexer,
				 param.PhraseKeyWordsInfoMap, hconf);
	} else {
		Analyzer *currentAnalyzer = AnalyzerFactory::getCurrentThreadAnalyzerWithSynonyms(server->indexDataConfig);
		this->highlightAlgorithms  = new AnalyzerBasedAlgorithm(currentAnalyzer,
				 param.PhraseKeyWordsInfoMap, hconf);
	}
	this->server = server;
	storedAttrSchema = Schema::create();
	RecordSerializerUtil::populateStoredSchema(storedAttrSchema, server->indexer->getSchema());
	compactRecDeserializer = new RecordSerializer(*storedAttrSchema);
	this->HighlightRecOffset = offset;
	this->HighlightRecCount = count;
	this->uncompressedInMemoryRecordString.reserve(4096);
}

ServerHighLighter::~ServerHighLighter() {
	delete this->compactRecDeserializer;
	delete this->highlightAlgorithms;
	delete storedAttrSchema;
    std::map<string, vector<unsigned> *>::iterator iter =
    						prefixToCompleteStore.begin();
    while(iter != prefixToCompleteStore.end()) {
    	delete iter->second;
    	++iter;
	}
}

} /* namespace httpwrapper */
} /* namespace srch2 */
