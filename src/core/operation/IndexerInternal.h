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

#pragma once
#ifndef __INDEXERINTERNAL_H__
#define __INDEXERINTERNAL_H__

#include <instantsearch/Indexer.h>
#include "operation/CacheManager.h"
#include "operation/IndexData.h"
#include <string>
#include <sstream>
#include <vector>
#include <memory>
#include "util/mypthread.h"
#include "index/FeedbackIndex.h"

using std::vector;
using std::string;

namespace srch2
{
namespace instantsearch
{
class CacheManager;
class GlobalCache;

struct IndexHealthInfo
{
    std::string lastMergeTimeString;
    unsigned doc_count;

    IndexHealthInfo()
    {
        this->getLatestHealthInfo(0);
    }

    void getString(struct std::tm *timenow, string &in)
    {
        char buffer [80];
        strftime (buffer,80,"%x %X",timenow);
        in = string(buffer);
    }

    void getLatestHealthInfo(unsigned doc_count)
    {
        time_t timer = time(NULL);
        struct std::tm* timenow = gmtime(&timer);
        IndexHealthInfo::getString(timenow, this->lastMergeTimeString);
        this->doc_count = doc_count;
    }

    const std::string getIndexHealthString() const
    {
        std::stringstream returnString;
        //returnString << "\"last_insert\":\"" << lastWriteTimeString << "\"";
        returnString << "\"last_merge\":\"" << lastMergeTimeString << "\"";
        returnString << ",\"doc_count\":\"" << doc_count << "\"";
        return returnString.str();
    }
};

class IndexReaderWriter: public Indexer
{
	friend void * dispatchMergeWorkerThread(void *arg);
public:

    //TODO put it as private
    pthread_mutex_t lockForWriters;

    IndexReaderWriter(IndexMetaData* indexMetaData, Analyzer *analyzer, Schema *schema);

    IndexReaderWriter(IndexMetaData* indexMetaData);

    void initIndexReaderWriter(IndexMetaData* indexMetaData);
    virtual ~IndexReaderWriter()
    {
    	if (this->mergeThreadStarted == true) {
	  pthread_mutex_lock(&lockForWriters);
	  this->mergeThreadStarted = false;
	  pthread_cond_signal(&countThresholdConditionVariable);
	  pthread_mutex_unlock(&lockForWriters);
        
	  pthread_join(mergerThread, NULL); // waiting to JOINABLE merge thread.
    	}
        delete this->index;
        delete this->userFeedbackIndex;
    };

    uint32_t getNumberOfDocumentsInIndex() const;

    /**
     * Builds the index. After commit(), the records are made searchable after the first commit.
     *
     * After the first commit, the call to commit does nothing.
     *
     */
    INDEXWRITE_RETVAL commit();

    /**
     * Adds a record. If primary key is duplicate, insert fails and -1 is returned. Otherwise, 0 is returned.
     */
    INDEXWRITE_RETVAL addRecord(const Record *record, Analyzer *analyzer);

    // Edits the records access list base on the command type
    INDEXWRITE_RETVAL aclRecordModifyRoles(const std::string &resourcePrimaryKeyID, vector<string> &roleIds, RecordAclCommandType commandType);

    // Deletes the role id from the permission map
    // we use this function for deleting a record from a role core
    // then we need to delete this record from the permission map of the resource cores of this core
    INDEXWRITE_RETVAL deleteRoleRecord(const std::string &rolePrimaryKeyID);

    /**
     * Deletes all the records.
     */
    INDEXWRITE_RETVAL deleteRecord(const std::string &primaryKeyID);

    INDEXWRITE_RETVAL deleteRecordGetInternalId(const std::string &primaryKeyID, unsigned &internalRecordId);

    INDEXWRITE_RETVAL recoverRecord(const std::string &primaryKeyID, unsigned internalRecordId);

    INDEXLOOKUP_RETVAL lookupRecord(const std::string &primaryKeyID);

    INDEXLOOKUP_RETVAL lookupRecord(const std::string &primaryKeyID, unsigned& internalRecordId);

    FeedbackIndex * getFeedbackIndexer() { return userFeedbackIndex; }

    /*
     * Note: Readers still need to call readerPreEnter and readerPreExit methods
     * before and after their operations.
     */
    inline const IndexData *getIndexData()
    {
        return this->index;
    }

    inline const void readerPreEnter(IndexReadStateSharedPtr_Token &readToken)
    {
    	// acquiring the global lock of readers and writers.
    	// NOTE: readerPreExit will unlock this call when invoked.
    	this->index->globalRwMutexForReadersWriters.lock_shared();
    	// Getting the protector copy of readview shared pointers.
    	// NOTE: These readview shared pointers are gone when the readToken
    	// gets destroyed.
        this->index->getReadView(readToken);
    }

    inline const void readerPreExit(IndexReadStateSharedPtr_Token &readToken)
    {
    	/*
    	 * readToken is released here. This object maintains the
    	 * guard copy of readview of all four indexes: Trie, II, FI, and QT.
    	 * As long as these shared pointer copies are not reset, readview copies
    	 * are not deallocated.
    	 * As of now, there is one readToken in the system which is
    	 * a member of QueryEvaluatorInternal. In main search/suggest query
    	 * execution process QueryEvaluator is constructed and deleted per query.
    	 * However one query evaluator object is used for multiple interactions with core
    	 * (query/insertions/deletions). This is why we can't use constructor/destructor
    	 * (which are nice locations) for storing/releasing the readviews.
    	 */
    	readToken.resetSharedPointers();
    	// Releasing the global lock of readers and writers.
    	this->index->globalRwMutexForReadersWriters.unlock_shared();
    	// Readviews will be erased when readToken is destructed.
    }

    inline const srch2::instantsearch::Schema *getSchema() const
    {
        return this->index->getSchema();
    }

    inline srch2::instantsearch::Schema *getSchema()
    {
        return this->index->getSchema();
    }

    inline StoredRecordBuffer getInMemoryData(unsigned internalRecordId) const
    {
        return this->index->getInMemoryData(internalRecordId);
    }

    const AttributeAccessControl & getAttributeAcl() const {
    	return *(this->index->attributeAcl);
    }
    void exportData(const string &exportedDataFileName);

    void save();

    void save(const std::string& directoryName);

    inline GlobalCache *getCache()
    {
        return this->cache;
    }

    inline const string getIndexHealth() const
    {
        std::stringstream str;
        str << "{\"engine_status\":{";
        str << "\"search_requests\":\"" << this->index->_getReadCount() << "\",";
        str << "\"write_requests\":\"" <<  this->index->_getWriteCount() << "\",";
        str << "\"docs_in_index\":\"" << this->index->_getNumberOfDocumentsInIndex() << "\",";
        str << this->indexHealthInfo.getIndexHealthString() << "}}";
        return str.str();
    }
    
    inline const bool isCommited() const { return this->index->isBulkLoadDone(); }


    // histogram update is triggered if :
    // A : we have had updateHistogramEveryQWrites writes since the last histogram update
    // or B : we have had updateHistogramEveryPMerges merges since the last histogram update
    inline bool shouldUpdateHistogram(){
    	if(writesCounterForMerge >= this->updateHistogramEveryQWrites ||
    			mergeCounterForUpdatingHistogram >= this->updateHistogramEveryPMerges){
    		return true;
    	}
    	return false;
    }

    inline void resetMergeCounterForHistogram(){
    	this->mergeCounterForUpdatingHistogram = 0;
    }

    inline void merge_ForTesting()
    {
        this->merge(false);
    }

    boost::shared_ptr<QuadTreeRootNodeAndFreeLists> getQuadTree_ReadView(){
    	boost::shared_ptr<QuadTreeRootNodeAndFreeLists> quadTreeRootNodeAndFreeLists;
    	this->index->quadTree->getQuadTreeRootNode_ReadView(quadTreeRootNodeAndFreeLists);
    	return quadTreeRootNodeAndFreeLists;
    }

    inline ForwardIndex * getForwardIndex() const { return this->index->forwardIndex; }

    pthread_t createAndStartMergeThreadLoop();

    void createAndStartMergeWorkerThreads();

    void startMergeThreadLoop();

private:
    IndexData *index;
    FeedbackIndex* userFeedbackIndex;
    CacheManager *cache;

    IndexHealthInfo indexHealthInfo;

    pthread_cond_t countThresholdConditionVariable;
    volatile bool mergeThreadStarted;

	pthread_t mergerThread;  // stores thread identifier.
	pthread_attr_t mergeThreadAttributes;  // store thread attributes

	pthread_t *mergerWorkerThreads;  // stores worker thread identifier.

    volatile unsigned writesCounterForMerge;
    bool needToSaveIndexes;
    unsigned mergeEveryNSeconds;
    unsigned mergeEveryMWrites;
    string indexDirectoryName;

    unsigned updateHistogramEveryPMerges;
    unsigned updateHistogramEveryQWrites;
    volatile unsigned mergeCounterForUpdatingHistogram;

    INDEXWRITE_RETVAL merge(bool updateHistogram);
    void doMerge();

};

}}

#endif /* __INDEXERINTERNAL_H__ */
