// $Id: AnalyzerInternalold.h 3377 2013-05-24 12:20:19Z huaijie $

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

#ifndef __CORE_ANALYZER__ANALYZERINTERNAL_H__
#define __CORE_ANALYZER__ANALYZERINTERNAL_H__

#include <instantsearch/Analyzer.h>
#include "analyzer/Normalizer.h"
#include "analyzer/StemmerInternal.h"
#include "analyzer/StandardAnalyzer.h"

//#include "Normalizer.h"
//#include "record/StemmerInternal.h"

#include <map>
#include <vector>
#include <string>

#include <boost/serialization/string.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/regex.hpp>

#include <fstream>

#include "util/Log.h"
#include "util/encoding.h"

using std::map;
using std::vector;
using std::string;

namespace srch2
{
namespace instantsearch
{

class AnalyzerInternalold : public Analyzer {
public:

    // for save-load
    AnalyzerInternalold();

    //AnalyzerInternalold(StemmerNormalizerType stemNormType = srch2::instantsearch::NO_STEMMER_NORMALIZER, std::string delimiters = " \r\n\t!%&*^@#,._+-=|/\\{}[]()?~`,<>;:\'\"");// Used by Analyzer.h
    AnalyzerInternalold(const StemmerNormalizerFlagType &stemmerFlag, const std::string &recordAllowedSpecialCharacters);// Used by Analyzer.h
    AnalyzerInternalold(const AnalyzerInternalold& analyzerInternal, const std::string &indexDirectory); //Used by IndexInternal.cpp constructor

    virtual ~AnalyzerInternalold();//{ };
    /*
     *  Analyzer allows a set of special characters in queries. These two functions are setter/getter
     *  for setting/getting the special characters.
     */
    void setRecordAllowedSpecialCharacters(const std::string &recordAllowedSpecialCharacters) { this->recordAllowedSpecialCharacters = recordAllowedSpecialCharacters;    }
    const std::string& getRecordAllowedSpecialCharacters()  const { return this->recordAllowedSpecialCharacters; }

    const StemmerNormalizerFlagType & getStemmerNormalizerFlagType() const
    {
        return this->stemmerFlag;
    };


    void prepareRegexExpression()
    {
    	//allow all characters
        string regexString = "[^A-Za-z0-9 " + this->recordAllowedSpecialCharacters +"\x80-\xFF" + "]";
        try{
            disallowedCharactersRegex = boost::regex(regexString);
        }
        catch(boost::regex_error& e) {
            
            std::cerr << regexString << " is not a valid regular expression. Using default: [^A-Za-z0-9 ]" << std::endl;
            disallowedCharactersRegex = boost::regex("[^A-Za-z0-9 ]");
        }

        multipleSpaceRegex = boost::regex(" +");
        headTailSpaceRegex = boost::regex("^[ \t]+|[ \t]+$");
    }

    //    /*TODO V1*/ AnalyzerInternal(const string &stopWordFile, const string &delimiters);
    //    /*TODO V1*/ AnalyzerInternal(const vector<string> &stopWords, const string &delimiters);

    /**
     * Function to tokenize a given record.
     * @param[in] record
     * @param[in, out] tokenAttributeHitsMap
     */
    void tokenizeRecord(const Record *record, map<string, TokenAttributeHits > &tokenAttributeHitsMap) const;

    /**
     * Function to tokenize a given query.
     * Remove duplicates like in query, "nose bleed nose" -> "nose bleed"
     * @param[in] queryString
     * @param[in, out] queryKeywords
     * @param[in] delimiterCharater
     */
    void tokenizeQuery(const string &queryString, vector<string> &queryKeywords, const char &delimiterCharater) const;

    void tokenizeQueryWithFilter(const string &queryString, vector<string> &queryKeywords, const char &delimiterCharacter,
                                 const char &filterDelimiterCharacter, const char &fieldsAndCharacter, const char &fieldsOrCharacter,
                                 const map<string, std::pair<unsigned, unsigned> > *searchableAttributesTriple, vector<unsigned> &filter) const;

    /**
     * Internal functions
     */
    static void load(AnalyzerInternalold &analyzerInternal, const std::string &analyzerFullPathFileName)
    {
        std::ifstream ifs(analyzerFullPathFileName.c_str(), std::ios::binary);
        boost::archive::binary_iarchive ia(ifs);
        ia >> analyzerInternal;
        analyzerInternal.prepareRegexExpression();
        ifs.close();
    };

    static void save(const AnalyzerInternalold &analyzerInternal, const std::string &analyzerFullPathFileName)
    {
        std::ofstream ofs(analyzerFullPathFileName.c_str(), std::ios::binary);
        boost::archive::binary_oarchive oa(ofs);
        oa << analyzerInternal;
        ofs.close();
    };

private:
    std::string recordAllowedSpecialCharacters;
    StemmerNormalizerFlagType stemmerFlag;
    Stemmer *stemmer;
    Normalizer *normalizer;
    AnalyzerInternal *analyzers;
    TokenOperator * tokenOperator;

    boost::regex disallowedCharactersRegex;
    boost::regex multipleSpaceRegex;
    boost::regex headTailSpaceRegex;

    friend class boost::serialization::access;

    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & stemmerFlag;
        ar & recordAllowedSpecialCharacters;
        ar & stemmer;
        ar & normalizer;
    }

    const string cleanString(const std::string &inputString) const;
};

}}

#endif /* __CORE_ANALYZER__ANALYZERINTERNAL_H__ */
