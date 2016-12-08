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

#ifndef __INCLUDE_INSTANTSEARCH__CONSTANTS_H__
#define __INCLUDE_INSTANTSEARCH__CONSTANTS_H__


#include "string"


namespace srch2 {
namespace instantsearch {

typedef unsigned CharType;
#define Byte char


/*
 * Example: tags is a multi valued attribute. Suppose one record has this value for tags :
 * ['C++ Coding Style','Java Concept Encapsulation','Programming Principles']
 * The value of this variable is used when the position of each token is assigned. We add this bump to tokens of each value of a multi-valued attribute
 * so that they are not matched in phrase-search. For example, adding this bump to 'Java' will prevent "Style Java" to match this record.
 */
const unsigned MULTI_VALUED_ATTRIBUTE_POSITION_BUMP = 100000;

static const char * MULTI_VAL_ATTR_DELIMITER = " $$ ";
const unsigned MULTI_VAL_ATTR_DELIMITER_LEN = 4;
/// Analyzer related constants
typedef enum {
    // there is no numbering for this enum. By default the numbers start from 0
    DISABLE_STEMMER_NORMALIZER, // Disables stemming
    ENABLE_STEMMER_NORMALIZER, // Enables stemming
    ONLY_NORMALIZER
} StemmerNormalizerFlagType;

typedef enum {
    DICTIONARY_PORTER_STEMMER
// We can add other kinds of stemmer here, like MIRROR_STEMMER

} StemmerType; // TODO: I should remove the '_' from the name, (it is temporary)

typedef enum {
    SYNONYM_KEEP_ORIGIN, // Disables stemming
    SYNONYM_DONOT_KEEP_ORIGIN   // Enables stemming
} SynonymKeepOriginFlag;


typedef enum {
    STANDARD_ANALYZER,    // StandardAnalyzer
    SIMPLE_ANALYZER,       // SimpleAnalyzer
    CHINESE_ANALYZER    // ChineseAnalyzer
} AnalyzerType;

// Enum values to indicate conjunction or disjunction operation among a data record attributes.
typedef enum {
	ATTRIBUTES_OP_AND,
	ATTRIBUTES_OP_OR,
	ATTRIBUTES_OP_NAND
} ATTRIBUTES_OP;

/// Faceted search filter

typedef enum{
	FacetTypeCategorical,
	FacetTypeRange,
	FacetTypeNonSpecified
} FacetType;



/// Indexer constants

typedef enum {
    OP_FAIL,
    OP_SUCCESS
} INDEXWRITE_RETVAL;

// Type of the acl query command
typedef enum{
	Acl_Record_Add,
	Acl_Record_Append,
	Acl_Record_Delete
}RecordAclCommandType;

/// Query constants
// TODO : change getAllResults in the code to FindAllResults
typedef enum
{
    SearchTypeTopKQuery ,
    SearchTypeGetAllResultsQuery ,
    SearchTypeRetrieveById
} QueryType;

//TODO add prefix OP
typedef enum
{
	LESS_THAN ,
	EQUALS,
	GREATER_THAN,
	LESS_THAN_EQUALS,
	GREATER_THAN_EQUALS,
	NOT_EQUALS

} AttributeCriterionOperation;


typedef enum
{
    SortOrderAscending ,
    SortOrderDescending,
    SortOrderNotSpecified
} SortOrder;

typedef enum{
	BooleanOperatorAND,
	BooleanOperatorOR,
	OP_NOT_SPECIFIED
} BooleanOperation;

///  Ranker constants



/// Record constants

typedef enum {
    LU_PRESENT_IN_READVIEW_AND_WRITEVIEW,
    LU_TO_BE_INSERTED,
    LU_ABSENT_OR_TO_BE_DELETED
} INDEXLOOKUP_RETVAL;

/// Schema constants

typedef enum
{
    DefaultIndex,
    LocationIndex
} IndexType;

// change the names, they are too general
typedef enum
{
    ATTRIBUTE_TYPE_INT,
    ATTRIBUTE_TYPE_LONG,
    ATTRIBUTE_TYPE_FLOAT,
    ATTRIBUTE_TYPE_DOUBLE,
    ATTRIBUTE_TYPE_TEXT,
    ATTRIBUTE_TYPE_TIME,// Time is kept as a long integer in the core.
    // The meaning of this long integer is the number of seconds past from January 1st, 1970
    // TypedValue class uses these constants to understand if it is dealing with a single-valued attribute
    // or a multi-valued one.
    ATTRIBUTE_TYPE_MULTI_INT,
    ATTRIBUTE_TYPE_MULTI_LONG,
    ATTRIBUTE_TYPE_MULTI_FLOAT,
    ATTRIBUTE_TYPE_MULTI_DOUBLE,
    ATTRIBUTE_TYPE_MULTI_TEXT,
    ATTRIBUTE_TYPE_MULTI_TIME,
    ATTRIBUTE_TYPE_DURATION
} FilterType;

/*typedef enum
{
    LUCENESCORE = 0,
    ABSOLUTESCORE = 1
} RecordScoreType;*/


typedef enum
{
    POSITION_INDEX_FULL,
    POSITION_INDEX_WORD , // the word offset of keyword in the record
    POSITION_INDEX_CHAR , // the character offset of keyword in the record
    POSITION_INDEX_FIELDBIT ,// keeps the attribute in which a keyword appears in
    POSITION_INDEX_NONE // For stemmer to work, positionIndex must be enabled.
} PositionIndexType;

bool inline isEnabledAttributeBasedSearch(PositionIndexType positionIndexType) {
	return (positionIndexType == POSITION_INDEX_FIELDBIT
			|| positionIndexType == POSITION_INDEX_WORD
			|| positionIndexType == POSITION_INDEX_CHAR
			|| positionIndexType == POSITION_INDEX_FULL);

}

bool inline isEnabledWordPositionIndex(PositionIndexType positionIndexType) {
	return (positionIndexType == POSITION_INDEX_WORD
			|| positionIndexType == POSITION_INDEX_FULL);
}

bool inline isEnabledCharPositionIndex(PositionIndexType positionIndexType) {
	return (positionIndexType == POSITION_INDEX_CHAR
			|| positionIndexType == POSITION_INDEX_FULL);
}
/// Term constants

typedef enum
{
    TERM_TYPE_PREFIX ,
    TERM_TYPE_COMPLETE ,
    TERM_TYPE_NOT_SPECIFIED,
    TERM_TYPE_PHRASE
} TermType;

///
enum DateTimeType{
    DateTimeTypeNow,
    DateTimeTypePointOfTime,
    DateTimeTypeDurationOfTime
};

/// response type
typedef enum
{
    RESPONSE_WITH_STORED_ATTR,
    RESPONSE_WITH_NO_STORED_ATTR,
    RESPONSE_WITH_SELECTED_ATTR
} ResponseType;

///
typedef enum
{
	FacetAggregationTypeCount
}FacetAggregationType;

typedef enum
{
	HistogramAggregationTypeSummation,
	HistogramAggregationTypeJointProbability
} HistogramAggregationType;

typedef enum {
	LogicalPlanNodeTypeAnd,
	LogicalPlanNodeTypeOr,
	LogicalPlanNodeTypeTerm,
	LogicalPlanNodeTypeNot,
	LogicalPlanNodeTypePhrase,
	LogicalPlanNodeTypeGeo
} LogicalPlanNodeType;

typedef enum {
	PhysicalPlanNode_NOT_SPECIFIED,
	PhysicalPlanNode_SortById,
	PhysicalPlanNode_SortByScore,
	PhysicalPlanNode_MergeTopK,
	PhysicalPlanNode_MergeSortedById,
	PhysicalPlanNode_MergeByShortestList,
	PhysicalPlanNode_UnionSortedById,
	PhysicalPlanNode_UnionLowestLevelTermVirtualList,
	PhysicalPlanNode_UnionLowestLevelSimpleScanOperator,
	PhysicalPlanNode_UnionLowestLevelSuggestion,
	PhysicalPlanNode_RandomAccessTerm,
	PhysicalPlanNode_RandomAccessAnd,
	PhysicalPlanNode_RandomAccessOr,
	PhysicalPlanNode_RandomAccessNot,
	PhysicalPlanNode_RandomAccessGeo,
	PhysicalPlanNode_GeoSimpleScan,
	PhysicalPlanNode_GeoNearestNeighbor,
	PhysicalPlanNode_Facet,
	PhysicalPlanNode_SortByRefiningAttribute,
	PhysicalPlanNode_FilterQuery,
	PhysicalPlanNode_PhraseSearch,
	PhysicalPlanNode_KeywordSearch,
	PhysicalPlanNode_FeedbackRanker
} PhysicalPlanNodeType;

typedef enum {
	PhysicalPlanIteratorProperty_SortById,
	PhysicalPlanIteratorProperty_SortByScore,
	// This value should not be included in output properties of any operator. It's a mechanism
	/// to always push down TVL operator and NULL operator.
	PhysicalPlanIteratorProperty_LowestLevel
} PhysicalPlanIteratorProperty;

}
}

namespace srch2is = srch2::instantsearch;

#endif // __INCLUDE_INSTANTSEARCH__CONSTANTS_H__
