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
 * AnalyzerFactory.cpp
 *
 *  Created on: Aug 21, 2013
 */

#include "AnalyzerFactory.h"
#include "ConfigManager.h"
#include <string>
#include <instantsearch/Analyzer.h>
#include <analyzer/AnalyzerContainers.h>
#include <index/IndexUtil.h>
#include <sys/stat.h>

#include <boost/thread/tss.hpp>

namespace srch2is = srch2::instantsearch;
using namespace srch2is;

namespace srch2 {
namespace httpwrapper {

// isSearcherThread = true indicates that the analyzer is being created for search. In such case
// we do not use synonym filter.
Analyzer* AnalyzerFactory::createAnalyzer(const CoreInfo_t* config, bool isSearcherThread) {

    // This flag shows if we need to stem or not. (StemmerNormalizerType is an enum)
    StemmerNormalizerFlagType stemmerFlag;
    // gets the stem flag and set the stemType
    if (config->getStemmerFlag()) {
        stemmerFlag = srch2is::ENABLE_STEMMER_NORMALIZER;
    } else {
        stemmerFlag = srch2is::DISABLE_STEMMER_NORMALIZER;
    }
    // This flag shows if we need to keep the origin word or not.
    SynonymKeepOriginFlag synonymKeepOriginFlag;
    if (config->getSynonymKeepOrigFlag()) {
        synonymKeepOriginFlag = srch2is::SYNONYM_KEEP_ORIGIN;
    } else {
        synonymKeepOriginFlag = srch2is::SYNONYM_DONOT_KEEP_ORIGIN;
    }

    // append the stemmer file to the install directory
    std::string stemmerFilePath = config->getStemmerFile();
    StemmerContainer *stemmer = NULL;
    if (stemmerFlag == ENABLE_STEMMER_NORMALIZER) {
            struct stat stResult;
            if (stat(stemmerFilePath.c_str(), &stResult) == 0) {
                stemmer = StemmerContainer::getInstance(stemmerFilePath);
            } else {
                stemmerFlag = DISABLE_STEMMER_NORMALIZER;
                Logger::error("The stemmer file %s is not valid. Please provide a valid file path." ,
                              stemmerFilePath.c_str());
            }
    }

    // gets the path of stopFilter
    std::string stopWordFilePath = config->getStopFilePath();
    if (stopWordFilePath.compare("") != 0) {
            struct stat stResult;
            if (stat(stopWordFilePath.c_str(), &stResult) != 0) {
                Logger::error("The stop word file %s is not valid. Please provide a valid file path.",
                              stopWordFilePath.c_str());
            }
    }
    StopWordContainer *stopWords = StopWordContainer::getInstance(stopWordFilePath);

    // gets the path of stopFilter
    SynonymContainer *synonyms = NULL;
    if (!isSearcherThread) {
        // Do not use synonym filter during search.
        std::string synonymFilePath = config->getSynonymFilePath();
        if (synonymFilePath.compare("") != 0) {
            struct stat stResult;
            if (stat(synonymFilePath.c_str(), &stResult) != 0) {
                Logger::error("The synonym file %s is not valid. Please provide a valid file path.",
                        synonymFilePath.c_str());
            }
        }
        synonyms = SynonymContainer::getInstance(synonymFilePath,
                synonymKeepOriginFlag);
    }

    std::string protectedWordFilePath = config->getProtectedWordsFilePath();
    if (protectedWordFilePath.compare("") != 0) {
            struct stat stResult;
            if (stat(protectedWordFilePath.c_str(), &stResult) != 0) {
                Logger::error("The protected word file %s is not valid. Please provide a valid file path.",
                              protectedWordFilePath.c_str());
            }
    }
    ProtectedWordsContainer *protectedWords = ProtectedWordsContainer::getInstance(protectedWordFilePath);

    AnalyzerType analyzerType = config->getAnalyzerType(); 
    std::string chineseDictFilePath = config->getChineseDictionaryPath();
    ChineseDictionaryContainer* chineseDictionaryContainer = NULL;
    if (analyzerType == CHINESE_ANALYZER){
        if (chineseDictFilePath.compare("") == 0){
            // If the user specifies to use the Chinese analyzer, but didn't provide the dictionary path
            // we will use the StandardAnalyzer instead.
            analyzerType = STANDARD_ANALYZER;
        } else {
            struct stat stResult;
            if (stat(chineseDictFilePath.c_str(), &stResult) != 0) {
                analyzerType = STANDARD_ANALYZER;
                Logger::error("The dictionary file %s is not valid. Please provide a valid file path.",
                              chineseDictFilePath.c_str());
            } else {
                chineseDictionaryContainer = ChineseDictionaryContainer::getInstance(chineseDictFilePath);
            }
        }
    }
    // Create an analyzer
    return new Analyzer(stemmer, stopWords, protectedWords, synonyms,
                            config->getRecordAllowedSpecialCharacters(),
                            analyzerType, chineseDictionaryContainer);
}

Analyzer* AnalyzerFactory::getCurrentThreadAnalyzer(const CoreInfo_t* config) {

    static boost::thread_specific_ptr<Analyzer> _ts_analyzer_object;
    if (_ts_analyzer_object.get() == NULL)
    {
        Logger::debug("Create Analyzer object for thread = %d ",  pthread_self());
        _ts_analyzer_object.reset(AnalyzerFactory::createAnalyzer(config, true));
    }

        /*
          TODO: This will fail if different types of analyzers are needed.  createAnalyzer() currently
          * defaults to always creating a StandardAnalyzer.  But if a later request is made for a
          * ChineseAnalyzer, as would happen if there are both English language and Chinese data sources
          * loaded in different cores, a previously created Analyzer of the wrong type could be returned.
          */
    Analyzer* analyzer = _ts_analyzer_object.get();

    // clear the initial states of the filters in the analyzer, e.g.,
    // for those filters that have an internal buffer to keep tokens.
    // Such an internal buffer can have leftover tokens from
    // the previous query (possibly an invalid query)
    analyzer->clearFilterStates();

    return analyzer;
}

Analyzer* AnalyzerFactory::getCurrentThreadAnalyzerWithSynonyms(const CoreInfo_t* config) {

    static boost::thread_specific_ptr<Analyzer> _tsAnalyzerObjectWithSynonyms;
    if (_tsAnalyzerObjectWithSynonyms.get() == NULL)
    {
        Logger::debug("Create Analyzer object for thread = %d ",  pthread_self());
        _tsAnalyzerObjectWithSynonyms.reset(AnalyzerFactory::createAnalyzer(config, false));
    }

    Analyzer* analyzer = _tsAnalyzerObjectWithSynonyms.get();

    // clear the initial states of the filters in the analyzer, e.g.,
    // for those filters that have an internal buffer to keep tokens.
    // Such an internal buffer can have leftover tokens from
    // the previous query (possibly an invalid query)
    analyzer->clearFilterStates();

    return analyzer;
}

void AnalyzerHelper::initializeAnalyzerResource (const CoreInfo_t* conf)
{
    // TODO - Move init() to getInstance() when we refactor this code
    if (conf->getProtectedWordsFilePath().compare("") != 0) {
        ProtectedWordsContainer::getInstance(conf->getProtectedWordsFilePath())->init();
    }
    if (conf->getSynonymFilePath().compare("") != 0) {
        SynonymKeepOriginFlag synonymKeepOriginFlag;
        if (conf->getSynonymKeepOrigFlag()) {
            synonymKeepOriginFlag = srch2is::SYNONYM_KEEP_ORIGIN;
        } else {
            synonymKeepOriginFlag = srch2is::SYNONYM_DONOT_KEEP_ORIGIN;
        }
        SynonymContainer::getInstance(conf->getSynonymFilePath(), synonymKeepOriginFlag)->init();
    }
    if (conf->getStemmerFile().compare("") != 0) {
        StemmerContainer::getInstance(conf->getStemmerFile())->init();
    }
    if (conf->getStopFilePath().compare("") != 0) {
        StopWordContainer::getInstance(conf->getStopFilePath())->init();
    }
    if (conf->getChineseDictionaryPath().compare("") != 0){
        ChineseDictionaryContainer::getInstance(conf->getChineseDictionaryPath())->init();
    }
}

void AnalyzerHelper::loadAnalyzerResource(const CoreInfo_t* conf) {
    try{
        const std::string& directoryName = conf->getIndexPath();
        std::ifstream ifs((directoryName + "/" + string(IndexConfig::analyzerFileName)).c_str(), std::ios::binary);
        if (ifs.good())
        {
            boost::archive::binary_iarchive ia(ifs);

            SynonymKeepOriginFlag synonymKeepOriginFlag;
            if (conf->getSynonymKeepOrigFlag()) {
                synonymKeepOriginFlag = srch2is::SYNONYM_KEEP_ORIGIN;
            } else {
                synonymKeepOriginFlag = srch2is::SYNONYM_DONOT_KEEP_ORIGIN;
            }
            SynonymContainer::getInstance(conf->getSynonymFilePath(), synonymKeepOriginFlag)->loadSynonymContainer(ia);
            StemmerContainer::getInstance(conf->getStemmerFile())->loadStemmerContainer(ia);
            StopWordContainer::getInstance(conf->getStopFilePath())->loadStopWordContainer(ia);
            ChineseDictionaryContainer::getInstance(conf->getChineseDictionaryPath())->loadDictionaryContainer(ia);
            ifs.close();
        }else {
            ifs.close();
            initializeAnalyzerResource(conf);
            saveAnalyzerResource(conf);
        }
                // TODO - Move init() to getInstance() when we refactor this code
        ProtectedWordsContainer::getInstance(conf->getProtectedWordsFilePath())->init();
    }catch (std::exception& ex){
        Logger::error("Error while loading Analyzer resource files");
        Logger::error(ex.what());
    }
}

void AnalyzerHelper::saveAnalyzerResource(const CoreInfo_t* conf) {
    try{
        const std::string& directoryName = conf->getIndexPath();
        std::ofstream ofs((directoryName + "/" + string(IndexConfig::analyzerFileName)).c_str(), std::ios::binary);
        if (ofs.good()) {
            boost::archive::binary_oarchive oa(ofs);

            SynonymKeepOriginFlag synonymKeepOriginFlag;
            if (conf->getSynonymKeepOrigFlag()) {
                synonymKeepOriginFlag = srch2is::SYNONYM_KEEP_ORIGIN;
            } else {
                synonymKeepOriginFlag = srch2is::SYNONYM_DONOT_KEEP_ORIGIN;
            }
            SynonymContainer::getInstance(conf->getSynonymFilePath(), synonymKeepOriginFlag)->saveSynonymContainer(oa);
            StemmerContainer::getInstance(conf->getStemmerFile())->saveStemmerContainer(oa);
            StopWordContainer::getInstance(conf->getStopFilePath())->saveStopWordContainer(oa);
            ChineseDictionaryContainer::getInstance(conf->getChineseDictionaryPath())
                ->saveDictionaryContainer(oa);
        }
        ofs.close();
    }catch(std::exception& ex){
        Logger::error("Error while saving Analyzer resource");
        Logger::error(ex.what());
    }
}

} // namesoace wrapper
} // namespace srch2


