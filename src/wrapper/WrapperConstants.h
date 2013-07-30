//$Id: Analyzer.h 3456 2013-06-14 02:11:13Z jiaying $

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

 * Copyright �� 2010 SRCH2 Inc. All rights reserved
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
    QueryBooleanOperatorFlag,
    KeywordFuzzyLevel,
    KeywordBoostLevel,
    FieldFilter,
    QueryPrefixCompleteFlag,
    IsDebugEnabled,
    ReponseAttributesList,
    ResultsStartOffset,
    NumberOfResults,
    MaxTimeAllowed,
    IsOmitHeader,
    ResponseFormat,
    FilterQueryEvaluatorFlag,
    TopKSearchType,
    GetAllResultsSearchType,
    GeoSearchType,
    // values related to search type specific parameters
    FacetQueryHandler,
    SortQueryHandler,
    GeoTypeRectangular,
    GeoTypeCircular
} ParameterName;

typedef enum {
    TimingDebug, QueryDebug, ResultsDebug, CompleteDebug
} QueryDebugLevel;

typedef enum {
    JSON
} ResponseResultsFormat;

typedef enum {
    MessageError, MessageWarning
} MessageType;

/// Configuration related constants

typedef enum {
    KAFKAWRITEAPI = 0, HTTPWRITEAPI = 1
} WriteApiType;

typedef enum {
    INDEXCREATE = 0, INDEXLOAD = 1
} IndexCreateOrLoad;

typedef enum {
    FILEBOOTSTRAP_FALSE = 0, FILEBOOTSTRAP_TRUE = 1
} DataSourceType;


}

}

namespace srch2http = srch2::httpwrapper;

#endif  // __INCLUDE_INSTANTSEARCH__WRAPPERCONSTANTS_H__
