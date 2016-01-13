// $Id: Analyzer.cpp 3219 2013-03-25 23:36:34Z iman $


#include "AnalyzerInternal.h"
#include "AnalyzerContainers.h"
#include "StandardAnalyzer.h"
#include "SimpleAnalyzer.h"
#include "ChineseAnalyzer.h"
#include <instantsearch/Analyzer.h>
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include <util/Assert.h>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>


using std::stringstream;

namespace srch2 {
namespace instantsearch {

Analyzer::Analyzer(const StemmerContainer *stemmer,
                   const StopWordContainer *stopWords,
                   const ProtectedWordsContainer *protectedWords,
                   const SynonymContainer *synonyms,
                   const std::string &allowedSpecialCharacters,
                   const AnalyzerType &analyzerType,
                   const ChineseDictionaryContainer* chineseDictionaryContainer
                   )
{
    switch (analyzerType) {
    case SIMPLE_ANALYZER:
        this->analyzerInternal = new SimpleAnalyzer(stemmer, stopWords, protectedWords, synonyms,
                                                    allowedSpecialCharacters);
        break;
    case CHINESE_ANALYZER:
        this->analyzerInternal = new ChineseAnalyzer(chineseDictionaryContainer,
                                                     stopWords, protectedWords, synonyms,
                                                     allowedSpecialCharacters);
        break;
    case STANDARD_ANALYZER:
    default:
        this->analyzerInternal = new StandardAnalyzer(stemmer, stopWords, protectedWords, synonyms,
                                                      allowedSpecialCharacters);
        break;
    }
    analyzerInternal->setTokenStream( analyzerInternal->createOperatorFlow());
}

Analyzer::Analyzer(Analyzer& analyzerObject) {
    switch (analyzerObject.analyzerInternal->getAnalyzerType()) {
    case SIMPLE_ANALYZER:
        this->analyzerInternal = new SimpleAnalyzer(
                static_cast<const SimpleAnalyzer&>(*(analyzerObject.analyzerInternal)));
        break;
    case CHINESE_ANALYZER:
        this->analyzerInternal = new ChineseAnalyzer(
                static_cast<const ChineseAnalyzer&>(*(analyzerObject.analyzerInternal)));
        break;
    case STANDARD_ANALYZER:
    default:
        this->analyzerInternal = new StandardAnalyzer(
                static_cast<const StandardAnalyzer&>(*(analyzerObject.analyzerInternal)));
        break;
    }
    analyzerInternal->setTokenStream( analyzerInternal->createOperatorFlow());
}

void Analyzer::setRecordAllowedSpecialCharacters(
        const std::string &recordAllowedSpecialCharacters) {
    this->analyzerInternal->setRecordAllowedSpecialCharacters(recordAllowedSpecialCharacters);
}

const std::string& Analyzer::getRecordAllowedSpecialCharacters() const {
    return this->analyzerInternal->getRecordAllowedSpecialCharacters();
}

void Analyzer::clearFilterStates(){
    this->analyzerInternal->clearFilterStates();
}

void Analyzer::tokenizeQuery(const std::string &queryString,
        std::vector<AnalyzedTermInfo> &queryKeywords,  bool isPrefix) const {
    this->analyzerInternal->tokenizeQuery(queryString, queryKeywords, isPrefix);
}

void Analyzer::tokenizeRecord(const Record *record,
        map<string, TokenAttributeHits> &tokenAttributeHitsMap) const {
    this->analyzerInternal->tokenizeRecord(record,tokenAttributeHitsMap);
}

AnalyzerType Analyzer::getAnalyzerType() const
{
    return this->analyzerInternal->getAnalyzerType();
}


void Analyzer::load(boost::archive::binary_iarchive &ia){
    this->analyzerInternal->load(ia);
}

void Analyzer::fillInCharacters(const char *data) {
	this->analyzerInternal->getTokenStream()->fillInCharacters(data);
}
bool Analyzer::processToken() {
	return this->analyzerInternal->getTokenStream()->processToken();
}
std::vector<CharType> & Analyzer::getProcessedToken() {
	return this->analyzerInternal->getTokenStream()->getProcessedToken();
}
unsigned Analyzer::getProcessedTokenCharOffset() {
	return this->analyzerInternal->getTokenStream()->getProcessedTokenCharOffset();
}
unsigned Analyzer::getProcessedTokenPosition() {
	return this->analyzerInternal->getTokenStream()->getProcessedTokenPosition();
}

unsigned Analyzer::getProcessedTokenLen() {
	return this->analyzerInternal->getTokenStream()->getProcessedTokenLen();
}

AnalyzedTokenType Analyzer::getProcessedTokenType() {
	return this->analyzerInternal->getTokenStream()->getProcessedTokentype();
}

void Analyzer::save(boost::archive::binary_oarchive &oa) {
    this->analyzerInternal->save(oa);
}

Analyzer::~Analyzer() {
    delete this->analyzerInternal;
}


}
}
