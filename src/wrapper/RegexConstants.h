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
#ifndef __REGEX_CONSTANTS__
#define __REGEX_CONSTANTS__

#include <string>

using namespace std;

namespace srch2 {
namespace httpwrapper {
// main query regex strings


// local parameter regex strings
const string LP_KEY_REGEX_STRING = "^\\s*[\\w_]+";
const string LP_KEY_VAL_DELIMETER_REGEX_STRING = "^\\s*=\\s*";
const string LP_VALUE_REGEX_STRING = "^[\\w_]+(,[\\w_]+)*";
// keyword regex strings
const string BOOST_REGEX_STRING = "\\^\\d+";
const string CHECK_FUZZY_NUMBER_REGEX_STRING = "^\\0?\\.\\d+";
const string NUMBERS_REGEX_STRING = "\\d+";
const string FIELD_AND_BOOL_OP_DELIMETER_REGEX_STRING = "\\.";
const string FIELD_OR_BOOL_OP_DELIMETER_REGEX_STRING = "\\+";
const string TERM_BOOL_OP_REGEX_STRING = "^(AND|OR)";
const string MAIN_QUERY_TERM_FIELD_REGEX_STRING = "^([\\w_]+((\\.|\\+)[\\w_]*)*|\\*)\\s*:";
const string MAIN_QUERY_KEYWORD_REGEX_STRING = "^[^\\*\\^\\~:]+";
const string MAIN_QUERY_ASTERIC_KEYWORD_REGEX_STRING = "^\\*\\s*";
const string PREFIX_MODIFIER_REGEX_STRING = "^\\*";
const string BOOST_MODIFIER_REGEX_STRING = "^\\^\\d*";
const string FUZZY_MODIFIER_REGEX_STRING = "^~((0?\\.(\\d)+)|1(\\.0+)?)?";
const string PHRASE_BOOST_MODIFIER_REGEX_STRING = "^\\^\\d+";
const string PROXIMITY_MODIFIER_REGEX_STRING = "^~\\d+";
const string FQ_FIELD_REGEX_STRING = "^-{0,1}[\\w_]+\\s*:";
const string COMPLEX_TERM_REGEX_STRING = "^boolexp\\$";

// filter query regex strings


const string FQ_TERM_BOOL_OP_REGEX_STRING = "^(AND|OR)";
const string FQ_RANGE_QUERY_KEYWORD_REGEX_STRING = "[^:]+\\]";
const string FQ_ASSIGNMENT_KEYWORD_REGEX_STRING = "^[^\\s:]+"; // no space allowed
const string FQ_COMPLEX_EXPRESSION_REGEX_STRING = "[^\\$]+";
const string FQ_FIELD_KEYWORD_DELIMETER_REGEX_STRING = "\\s+TO\\s+";
}

/* Query field boost regex strings, used to match string in the qf= section:
   matches from the begining of string up to and including the first ^.
     Use cases are as follows: qf=title^100+body^100
       - first regex call gets string title^100+body^100
         and this expression matches title^
       - second regex call get string body^100 and this expression
         matches body^ */
const string QF_ATTRIBUTE_REGEX_STRING = "^[^\\^]*\\^";
}

#endif // __REGEX_CONSTANTS__
