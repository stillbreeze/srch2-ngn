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

#ifndef __INCLUDE_INSTANTSEARCH__WRAPPERCONSTANTS_H__
#define __INCLUDE_INSTANTSEARCH__WRAPPERCONSTANTS_H__

#include <instantsearch/Constants.h>

namespace srch2 {
namespace httpwrapper {

/// Filter query constants

/// Parse related constants

typedef enum {

    RawQueryKeywords,
    IsFuzzyFlag,
    LengthBoostFlag,
    PrefixMatchPenaltyFlag,
    KeywordSimilarityThreshold,
    KeywordBoostLevel,
    FieldFilter,
    AccessControl,
    QueryPrefixCompleteFlag,
    IsPhraseKeyword,
    IsDebugEnabled,
    ReponseAttributesList,
    ResultsStartOffset,
    NumberOfResults,
    MaxTimeAllowed,
    IsOmitHeader,
    ResponseFormat,
    GeoSearchFlag,
    FilterQueryEvaluatorFlag,
    RetrieveByIdSearchType,
    TopKSearchType,
    GetAllResultsSearchType,
    // values related to search type specific parameters
    FacetQueryHandler,
    SortQueryHandler,
    GeoTypeRectangular,
    GeoTypeCircular,
    QueryFieldBoostFlag
} ParameterName;

typedef enum {
    TimingDebug, QueryDebug, ResultsDebug, CompleteDebug
} QueryDebugLevel;

typedef enum {
    JSON
} ResponseResultsFormat;

typedef enum {
    MessageError, MessageWarning, MessageNotice
} MessageType;

/// Configuration related constants

typedef enum {
	HTTPWRITEAPI
} WriteApiType;

typedef enum {
	INDEXCREATE ,
	INDEXLOAD
} IndexCreateOrLoad;

typedef enum {
	DATA_SOURCE_NOT_SPECIFIED,
	DATA_SOURCE_JSON_FILE,
	DATA_SOURCE_DATABASE
} DataSourceType;

}

}

namespace srch2http = srch2::httpwrapper;

#endif  // __INCLUDE_INSTANTSEARCH__WRAPPERCONSTANTS_H__
