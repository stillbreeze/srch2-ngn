//$Id: JSONRecordParser.h 3456 2013-06-14 02:11:13Z jiaying $

#ifndef _DAEMONDATASOURCE_H_
#define _DAEMONDATASOURCE_H_

#include "json/json.h"
#include <instantsearch/Schema.h>
#include <instantsearch/Analyzer.h>
#include <instantsearch/Record.h>
#include <instantsearch/Indexer.h>
#include "util/RecordSerializer.h"

#include "ConfigManager.h"

namespace srch2is = srch2::instantsearch;

namespace srch2 {
namespace httpwrapper {

class JSONRecordParser {
public:
    static bool populateRecordFromJSON(const std::string &inputLine,
            const CoreInfo_t *indexDataContainerConf, srch2is::Record *record,
            std::stringstream &error, RecordSerializer& compactRecSerializer);
    static bool _JSONValueObjectToRecord(srch2is::Record *record,
            const std::string &inputLine, const Json::Value &root,
            const CoreInfo_t *indexDataContainerConf, std::stringstream &error,
            RecordSerializer& compactRecSerializer);
    static srch2is::Schema* createAndPopulateSchema(
            const CoreInfo_t *indexDataContainerConf);

private:
    static void getJsonValueString(const Json::Value &jsonValue,
            const std::string &key, std::vector<std::string> &stringValue,
            const string &configName);
    static void getJsonValueDateAndTime(const Json::Value &jsonValue,
            const std::string &key, std::vector<std::string> &stringValue,
            const string &configName);
    static void getJsonValueDouble(const Json::Value &jsonValue,
            const std::string &key, double &doubleValue,
            const string& configName);
    static bool setRecordPrimaryKey(srch2is::Record *record,
            const Json::Value &root, const CoreInfo_t *indexDataContainerConf,
            std::stringstream &error);
    static bool setRecordSearchableAttr(const Json::Value &root,
            RecordSerializer& compactRecSerializer);
    static bool setRecordRefiningAttr(const Json::Value &root,
            RecordSerializer& compactRecSerializer);
    static bool setRecordSearchableValue(srch2is::Record *record,
            const Json::Value &root, const CoreInfo_t *indexDataContainerConf,
            std::stringstream &error);
    static bool setRecordRefiningValue(srch2is::Record *record,
            const Json::Value &root, const CoreInfo_t *indexDataContainerConf,
            std::stringstream &error);
    static bool setRecordLocationValue(srch2is::Record *record,
            const Json::Value &root, const CoreInfo_t *indexDataContainerConf);
    static bool setRecordBoostValue(srch2is::Record *record,
            const Json::Value &root, const CoreInfo_t *indexDataContainerConf);
    static bool stringToUnsigned(const char* str, unsigned & output);
};

class DaemonDataSource {
public:
    static unsigned createNewIndexFromFile(srch2is::Indexer *indexer,
            Schema * storedAttrSchema,
            const CoreInfo_t *indexDataContainerConf);
};

}
}

#endif // _DAEMONDATASOURCE_H_
