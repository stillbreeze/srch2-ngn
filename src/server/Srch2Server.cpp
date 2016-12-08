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
#include <syslog.h>
#include "Srch2Server.h"
#include "util/RecordSerializerUtil.h"
#include "operation/AttributeAccessControl.h"

#ifndef ANDROID
#   include <sys/statvfs.h>
#else
#   include <sys/vfs.h>
#   define statvfs statfs
#endif


namespace srch2 {
namespace httpwrapper {

//Helper function to calculate size of the file
namespace {
    ifstream::pos_type getFileSize(const char* filename) {
        std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
        ifstream::pos_type size = in.tellg();
        in.close();
        return size;
    }
}

const char *HTTPServerEndpoints::index_search_url = "/srch2/search";
const char *HTTPServerEndpoints::index_info_url = "/srch2/info";
const char *HTTPServerEndpoints::cache_clear_url = "/srch2/clear";
const char *HTTPServerEndpoints::index_insert_url = "/srch2/index/insert";
const char *HTTPServerEndpoints::index_delete_url = "/srch2/index/delete";
const char *HTTPServerEndpoints::index_save_url = "/srch2/index/save";
const char *HTTPServerEndpoints::index_merge_url = "/srch2/index/merge";
const char *HTTPServerEndpoints::index_stop_url = "/srch2/stop";

/*const char *ajax_reply_start =
 "HTTP/1.1 200 OK\r\n"
 "Cache: no-cache\r\n"
 "Content-Type: application/x-javascript\r\n"
 "\r\n";*/

/*const char *HTTPServerEndpoints::ajax_search_pass =
 "HTTP/1.1 200 OK\r\n"
 "Cache: no-cache\r\n"
 "Content-Type: application/json\r\n"
 "Content-Length:%s\n"
 "\r\n";*/

const char *HTTPServerEndpoints::ajax_search_fail =
        "HTTP/1.1 400 Bad Request\r\n"
                "Cache: no-cache\r\n"
                "Content-Type: application/x-javascript\r\n"
                "\r\n";

const char *HTTPServerEndpoints::ajax_insert_pass = "HTTP/1.1 201 Created\r\n"
        "Cache: no-cache\r\n"
        "Content-Type: application/x-javascript\r\n"
        "\r\n";

const char *HTTPServerEndpoints::ajax_insert_fail =
        "HTTP/1.1 400 Bad Request\r\n"
                "Cache: no-cache\r\n"
                "Content-Type: application/x-javascript\r\n"
                "\r\n";

const char *HTTPServerEndpoints::ajax_insert_fail_403 =
        "HTTP/1.1 403 Forbidden\r\n"
                "Cache: no-cache\r\n"
                "Content-Type: application/x-javascript\r\n"
                "\r\n";

const char *HTTPServerEndpoints::ajax_insert_fail_500 =
        "HTTP/1.1 500 Internal Server Error\r\n"
                "Cache: no-cache\r\n"
                "Content-Type: application/x-javascript\r\n"
                "\r\n";

const char *HTTPServerEndpoints::ajax_save_pass = "HTTP/1.1 200 OK\r\n"
        "Cache: no-cache\r\n"
        "Content-Type: application/x-javascript\r\n"
        "\r\n";

const char *HTTPServerEndpoints::ajax_merge_pass = "HTTP/1.1 200 OK\r\n"
        "Cache: no-cache\r\n"
        "Content-Type: application/x-javascript\r\n"
        "\r\n";

//TODO: NO way to tell if save failed on srch2 index
/*const char *ajax_save_fail =
 "HTTP/1.1 400 Bad Request\r\n"
 "Cache: no-cache\r\n"
 "Content-Type: application/x-javascript\r\n"
 "\r\n";*/

const char *HTTPServerEndpoints::ajax_delete_pass = "HTTP/1.1 200 OK\r\n"
        "Cache: no-cache\r\n"
        "Content-Type: application/x-javascript\r\n"
        "\r\n";

const char *HTTPServerEndpoints::ajax_delete_fail =
        "HTTP/1.1 400 Bad Request\r\n"
                "Cache: no-cache\r\n"
                "Content-Type: application/x-javascript\r\n"
                "\r\n";

const char *HTTPServerEndpoints::ajax_delete_fail_500 =
        "HTTP/1.1 500 Internal Server Error\r\n"
                "Cache: no-cache\r\n"
                "Content-Type: application/x-javascript\r\n"
                "\r\n";


bool Srch2Server::checkIndexExistence(const CoreInfo_t *indexDataConfig) {
    const string &directoryName = indexDataConfig->getIndexPath();
    if (!checkDirExistence(
            (directoryName + "/" + IndexConfig::analyzerFileName).c_str()))
        return false;
    if (!checkDirExistence(
            (directoryName + "/" + IndexConfig::trieFileName).c_str()))
        return false;
    if (!checkDirExistence(
            (directoryName + "/" + IndexConfig::forwardIndexFileName).c_str()))
        return false;
    if (!checkDirExistence(
            (directoryName + "/" + IndexConfig::schemaFileName).c_str()))
        return false;
    if (indexDataConfig->getIndexType() == srch2::instantsearch::DefaultIndex) {
        // Check existence of the inverted index file for basic keyword search ("A1")
        if (!checkDirExistence(
                (directoryName + "/" + IndexConfig::invertedIndexFileName).c_str()))
            return false;
    } else {
        // Check existence of the quadtree index file for geo keyword search ("M1")
        if (!checkDirExistence(
                (directoryName + "/" + IndexConfig::quadTreeFileName).c_str()))
            return false;
    }
    return true;
}

IndexMetaData *Srch2Server::createIndexMetaData(
        const CoreInfo_t *indexDataConfig) {
    //Create a cache
    srch2is::GlobalCache *cache = srch2is::GlobalCache::create(
            indexDataConfig->getCacheSizeInBytes(), 200000);

    // Create an IndexMetaData
    srch2is::IndexMetaData *indexMetaData = new srch2is::IndexMetaData(cache,
            indexDataConfig->getMergeEveryNSeconds(),
            indexDataConfig->getMergeEveryMWrites(),
            indexDataConfig->getUpdateHistogramEveryPMerges(),
            indexDataConfig->getUpdateHistogramEveryQWrites(),
            indexDataConfig->getIndexPath());

    indexMetaData->maxFeedbackRecordsPerQuery = indexDataConfig->getMaxFeedbackRecordsPerQuery();
    indexMetaData->maxCountOfFeedbackQueries = indexDataConfig->getMaxFeedbackQueriesCount();
    return indexMetaData;
}
void Srch2Server::createHighlightAttributesVector(
        const srch2is::Schema * schema) {

    CoreInfo_t *indexConfig = const_cast<CoreInfo_t *>(this->indexDataConfig);
    vector<std::pair<unsigned, string> > highlightAttributes;

    const map<string, SearchableAttributeInfoContainer> * searchableAttrsFromConfig =
            indexConfig->getSearchableAttributes();

    map<string, SearchableAttributeInfoContainer>::const_iterator cIter;

    std::map<std::string, unsigned>::const_iterator iter =
            schema->getSearchableAttribute().begin();
    for (; iter != schema->getSearchableAttribute().end(); iter++) {
        // Currently only searchable attributes are highlightable. Cross validate the schema
        // attribute with configuration attributes. (There could be a mismatch when index is loaded
        // from file).
        cIter = searchableAttrsFromConfig->find(iter->first);
        if (cIter != searchableAttrsFromConfig->end()
                && cIter->second.highlight) {
            highlightAttributes.push_back(make_pair(iter->second, iter->first));
        }
    }
    indexConfig->setHighlightAttributeIdsVector(highlightAttributes);
}
void Srch2Server::createAndBootStrapIndexer() {
    // create IndexMetaData
    IndexMetaData *indexMetaData = createIndexMetaData(this->indexDataConfig);
    IndexCreateOrLoad indexCreateOrLoad;
    if (checkIndexExistence(indexDataConfig))
        indexCreateOrLoad = srch2http::INDEXLOAD;
    else
        indexCreateOrLoad = srch2http::INDEXCREATE;
    Schema * storedAttrSchema = Schema::create();
    // Create a schema to the data source definition in the Srch2ServerConf
    srch2is::Schema *schema = JSONRecordParser::createAndPopulateSchema(
            indexDataConfig);

    switch (indexCreateOrLoad) {
    case srch2http::INDEXCREATE: {
        AnalyzerHelper::initializeAnalyzerResource(this->indexDataConfig);

        Analyzer *analyzer = AnalyzerFactory::createAnalyzer(
                this->indexDataConfig);
        indexer = Indexer::create(indexMetaData, analyzer, schema);
        delete analyzer;
        RecordSerializerUtil::populateStoredSchema(storedAttrSchema,
                indexer->getSchema());
        switch (indexDataConfig->getDataSourceType()) {
        case srch2http::DATA_SOURCE_JSON_FILE: {

            //check file size in KB
            unsigned fileSize = getFileSize(indexDataConfig->getDataFilePath().c_str());
            //Logger::console("The size of the data file is %lu KB", fileSize/(1024));

            struct statvfs *buff;
            if (!(buff = (struct statvfs *) malloc(sizeof(struct statvfs)))) {
                Logger::error("Failed to allocate memory to buffer.");
            } else {
                //We check the space available for the disk where srch2Home is set
                if (statvfs(indexDataConfig->getSrch2Home().c_str(), buff) < 0) {
                    Logger::warn("Failed to calculate free disk space, statvfs() failed.");
                } else {
                    //Logger::console("The number of free blocks on disk is %lu", buff->f_bfree);
                    //Logger::console("The size of each block is %lu bytes", buff->f_bsize);
                    //Logger::console("The total size of free disk space is %lu KB", (buff->f_bfree * buff->f_bsize) / (1024));

                    //calculate free disk space. (No. of free blocks * block size) KB
                    unsigned long freeDiskSpace = (buff->f_bfree * buff->f_bsize) / (1024);

                    //Display warning if free disk space is less than twice the size of file
                    if (freeDiskSpace < (2 * fileSize)) {
                        Logger::warn("The system may not have enough disk space to serialize the indexes for the given json file.");
                    }
                }
                free(buff);
            }

            // Create from JSON and save to index-dir
            Logger::console("%s: Creating indexes from JSON file...",this->coreName.c_str());
            unsigned indexedCounter = DaemonDataSource::createNewIndexFromFile(
                    indexer, storedAttrSchema, indexDataConfig);
            /*
             *  commit the indexes once bulk load is done and then save it to the disk only
             *  if number of indexed record is > 0.
             */
            indexer->commit();

            // Load ACL list from disk
            indexer->getAttributeAcl().bulkLoadAttributeAclJSON(indexDataConfig->getAttibutesAclFile());

            // Load record-based acl data
            if(indexDataConfig->getHasRecordAcl()){
            	DaemonDataSource::addRecordAclFile(indexer, indexDataConfig);
            }

            /*
             *  if the roleCore is null it means that this core doesn't have any access control
             *  so we can save it now.
             *  otherwise first we should read the data for acl and we will save this core after that.
             */
            if (indexedCounter > 0) {
                indexer->save();
                Logger::console("Indexes saved.");
            }
            break;
        }
        default: {
            indexer->commit();
            // Load ACL list from disk
            indexer->getAttributeAcl().bulkLoadAttributeAclJSON(indexDataConfig->getAttibutesAclFile());
            Logger::console("Creating new empty index");
        }
        };
        AnalyzerHelper::saveAnalyzerResource(this->indexDataConfig);
        break;
    }
    case srch2http::INDEXLOAD: {
        // Load from index-dir directly, skip creating an index initially.
        indexer = Indexer::load(indexMetaData);

        if (!checkSchemaConsistency(schema, indexer->getSchema())) {
            Logger::warn("The schema in the config file is different from the"
                    " serialized schema on the disk. The engine will ignore "
                    "the schema from the config file. Please make sure they "
                    "are consistent. One possible solution is to remove all "
                    "the index files and run the engine again.");
        }

        // Load Analayzer data from disk
        AnalyzerHelper::loadAnalyzerResource(this->indexDataConfig);
        indexer->getSchema()->setSupportSwapInEditDistance(
                indexDataConfig->getSupportSwapInEditDistance());
        bool isAttributeBasedSearch = false;
        if (isEnabledAttributeBasedSearch(
                indexer->getSchema()->getPositionIndexType())) {
            isAttributeBasedSearch = true;
        }
        if (isAttributeBasedSearch
                != indexDataConfig->getSupportAttributeBasedSearch()) {
            Logger::warn(
                    "support-attribute-based-search has changed in the config file"
                            " remove all index files and run it again!");
        }
        RecordSerializerUtil::populateStoredSchema(storedAttrSchema,
                indexer->getSchema());
        break;
    }
    }
    createHighlightAttributesVector(storedAttrSchema);
    delete storedAttrSchema;
    delete schema;
    // start merger thread
    indexer->createAndStartMergeThreadLoop();
}

/*
 * This function will check the consistency of the schema that is loaded from the
 * disk and the schema that is loaded from the config file.
 */
bool Srch2Server::checkSchemaConsistency(srch2is::Schema *confSchema,
        srch2is::Schema *loadedSchema) {
    if (confSchema->getNumberOfRefiningAttributes()
            != loadedSchema->getNumberOfRefiningAttributes()) {
        return false;
    }

    if (confSchema->getNumberOfSearchableAttributes()
            != loadedSchema->getNumberOfSearchableAttributes()) {
        return false;
    }

    for (std::map<std::string, unsigned>::const_iterator confIt =
            confSchema->getRefiningAttributes()->begin(), loadedIt =
            loadedSchema->getRefiningAttributes()->begin();
            confIt != confSchema->getRefiningAttributes()->end()
                    && loadedIt != loadedSchema->getRefiningAttributes()->end();
            confIt++, loadedIt++) {
        //Compare the refining attribute's name and type to see if they are same.
        if (confIt->first.compare(loadedIt->first) != 0) {
            return false;
        }

        if (confSchema->getTypeOfRefiningAttribute(confIt->second)
                != loadedSchema->getTypeOfRefiningAttribute(loadedIt->second)) {
            return false;
        }
    }
    for (std::map<std::string, unsigned>::const_iterator confIt =
            confSchema->getSearchableAttribute().begin(), loadedIt =
            loadedSchema->getSearchableAttribute().begin();
            confIt != confSchema->getSearchableAttribute().end()
                    && loadedIt != loadedSchema->getSearchableAttribute().end();
            confIt++, loadedIt++) {
        //Compare the searchable attribute's name and type to see if they are same.
        if (confIt->first.compare(loadedIt->first) != 0) {
            return false;
        }

        if (confSchema->getTypeOfSearchableAttribute(confIt->second)
                != loadedSchema->getTypeOfSearchableAttribute(
                        loadedIt->second)) {
            return false;
        }
    }
    return true;
}

void Srch2Server::setCoreName(const string &name) {
    coreName = name;
}

const string &Srch2Server::getCoreName() {
    return coreName;
}

}
}
