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


#include "operation/IndexerInternal.h"
#include "util/Logger.h"

namespace srch2
{
namespace instantsearch
{
    
INDEXWRITE_RETVAL IndexReaderWriter::commit()
{    
    pthread_mutex_lock(&lockForWriters);
    INDEXWRITE_RETVAL commitReturnValue;
    if (!this->index->isBulkLoadDone()) {
    	commitReturnValue = this->index->finishBulkLoad();
    	this->userFeedbackIndex->finalize();
    } else {
    	/*
    	 *  If bulk load is done, then we are in past bulk load stage. We should call merge function
    	 *  even if dedicated merge thread running. The rwMutexLock will take care of concurrency.
    	 */
    	bool updateHistogramFlag = shouldUpdateHistogram();
    	if(updateHistogramFlag == true){
    		this->resetMergeCounterForHistogram();
    	}
    	commitReturnValue =this->merge(updateHistogramFlag);

    }
    this->writesCounterForMerge = 0;
    
    pthread_mutex_unlock(&lockForWriters);

    return commitReturnValue;
}

INDEXWRITE_RETVAL IndexReaderWriter::addRecord(const Record *record, Analyzer* analyzer)
{
    pthread_mutex_lock(&lockForWriters);
    INDEXWRITE_RETVAL returnValue = this->index->_addRecord(record, analyzer);
    if (returnValue == OP_SUCCESS) {
    	this->writesCounterForMerge++;
    	this->needToSaveIndexes = true;
    	if (this->mergeThreadStarted && writesCounterForMerge >= mergeEveryMWrites) {
        pthread_cond_signal(&countThresholdConditionVariable);
    	}
    }

    pthread_mutex_unlock(&lockForWriters);
    return returnValue;
}

INDEXWRITE_RETVAL IndexReaderWriter::aclRecordModifyRoles(const std::string &resourcePrimaryKeyID, vector<string> &roleIds, RecordAclCommandType commandType)
{
	pthread_mutex_lock(&lockForWriters);

	INDEXWRITE_RETVAL returnValue = this->index->_aclModifyRecordAccessList(resourcePrimaryKeyID, roleIds, commandType);

	// By editing the access list of a record the result of a search could change
	// So we need to clear the cache.
	if(returnValue == OP_SUCCESS){
	    if (this->cache != NULL)
	        this->cache->clear();
	    this->needToSaveIndexes = true;
	}

	pthread_mutex_unlock(&lockForWriters);
	return returnValue;
}

INDEXWRITE_RETVAL IndexReaderWriter::deleteRoleRecord(const std::string &rolePrimaryKeyID){
	pthread_mutex_lock(&lockForWriters);

	INDEXWRITE_RETVAL returnValue = this->index->_aclRoleRecordDelete(rolePrimaryKeyID);

	if(returnValue == OP_SUCCESS){
		if (this->cache != NULL)
			this->cache->clear();
		this->needToSaveIndexes = true;
	}

	pthread_mutex_unlock(&lockForWriters);
	return returnValue;
}

INDEXWRITE_RETVAL IndexReaderWriter::deleteRecord(const std::string &primaryKeyID)
{
    unsigned internalRecordId;
    return this->deleteRecordGetInternalId(primaryKeyID, internalRecordId);
}

INDEXWRITE_RETVAL IndexReaderWriter::deleteRecordGetInternalId(const std::string &primaryKeyID, unsigned &internalRecordId)
{
    pthread_mutex_lock(&lockForWriters);

    // if the trie needs to reassign ids, we need to call merge() first to make sure
    // those keyword ids are preserving order, since the logic inside
    // deleteRecord() to find leaf nodes of the keywords in the deleted
    // record relies on this order.
    if (this->index->trie->needToReassignKeywordIds()) {
        this->doMerge();
    }

    INDEXWRITE_RETVAL returnValue = this->index->_deleteRecordGetInternalId(primaryKeyID, internalRecordId);
    if (returnValue == OP_SUCCESS) {
    	this->writesCounterForMerge++;
    	this->needToSaveIndexes = true;
    	if (this->mergeThreadStarted && writesCounterForMerge >= mergeEveryMWrites){
    		pthread_cond_signal(&countThresholdConditionVariable);
    	}
    }

    pthread_mutex_unlock(&lockForWriters);
    return returnValue;
}

INDEXWRITE_RETVAL IndexReaderWriter::recoverRecord(const std::string &primaryKeyID, unsigned internalRecordId)
{
    pthread_mutex_lock(&lockForWriters);

    INDEXWRITE_RETVAL returnValue = this->index->_recoverRecord(primaryKeyID, internalRecordId);
    if (returnValue == OP_SUCCESS) {
    	this->writesCounterForMerge++;
    	this->needToSaveIndexes = true;
    	if (this->mergeThreadStarted && writesCounterForMerge >= mergeEveryMWrites) {
    		pthread_cond_signal(&countThresholdConditionVariable);
    	}
    }

    pthread_mutex_unlock(&lockForWriters);
    return returnValue;
}

INDEXLOOKUP_RETVAL IndexReaderWriter::lookupRecord(const std::string &primaryKeyID) {
	unsigned internalRecordId;
	return lookupRecord(primaryKeyID, internalRecordId);
}
INDEXLOOKUP_RETVAL IndexReaderWriter::lookupRecord(const std::string &primaryKeyID, unsigned& internalRecordId)
{
    // although it's a read-only OP, since we need to check the writeview
    // we need to acquire the writelock
    // but do NOT need to increase the merge counter

    pthread_mutex_lock(&lockForWriters);
    
    INDEXLOOKUP_RETVAL returnValue = this->index->_lookupRecord(primaryKeyID, internalRecordId);

    pthread_mutex_unlock(&lockForWriters);

    return returnValue;
}

void IndexReaderWriter::exportData(const string &exportedDataFileName)
{
    // add write lock
    pthread_mutex_lock(&lockForWriters);

    // merge the index
    // we don't have to update histogram information when we want to export.
    this->merge(false);
    writesCounterForMerge = 0;

    //get the export data
    this->index->_exportData(exportedDataFileName);

    // free write lock
    pthread_mutex_unlock(&lockForWriters);
}

void IndexReaderWriter::save()
{
    pthread_mutex_lock(&lockForWriters);

    // If no insert/delete/update is performed, we don't need to save.
    bool needToSaveFeedbackIndex = userFeedbackIndex->getSaveIndexFlag();
    if(this->needToSaveIndexes == false && needToSaveFeedbackIndex == false){
      pthread_mutex_unlock(&lockForWriters);
    	return;
    }

    // we don't have to update histogram information when we want to export.
    this->merge(false);
    writesCounterForMerge = 0;

    srch2::util::Logger::console("Saving Indexes ...");
    if (this->needToSaveIndexes)
    	this->index->_save(this->cache);
    if (needToSaveFeedbackIndex)
    	this->userFeedbackIndex->save(indexDirectoryName);

    // Since one save is done, we need to set needToSaveIndexes back to false
    // we need this line because of bulk-load. Because in normal save, the engine will be killed after save
    // so we don't need this flag (the engine will die anyways); but in the save which happens after bulk-load,
    // we should set this flag back to false for future save calls.
    this->needToSaveIndexes = false;
    userFeedbackIndex->setSaveIndexFlag(false);

    pthread_mutex_unlock(&lockForWriters);
}

void IndexReaderWriter::save(const std::string& directoryName)
{
    pthread_mutex_lock(&lockForWriters);

    // we don't have to update histogram information when we want to export.
    this->merge(false);
    writesCounterForMerge = 0;

    this->index->_save(this->cache, directoryName);

    pthread_mutex_unlock(&lockForWriters);
}

/*
 *  This function is not thread safe. Caller of this function should hold valid lock. This
 *  function should not be exposed outside core.
 */

INDEXWRITE_RETVAL IndexReaderWriter::merge(bool updateHistogram)
{
    if (this->cache != NULL && this->index->isMergeRequired())
        this->cache->clear();

    // increment the mergeCounterForUpdatingHistogram
    this->mergeCounterForUpdatingHistogram ++;

    struct timespec tstart;
    clock_gettime(CLOCK_REALTIME, &tstart);

    this->userFeedbackIndex->merge();

    INDEXWRITE_RETVAL returnValue = this->index->_merge(this->cache, updateHistogram);

    struct timespec tend;
    clock_gettime(CLOCK_REALTIME, &tend);
    unsigned time = (tend.tv_sec - tstart.tv_sec) * 1000 + (tend.tv_nsec - tstart.tv_nsec) / 1000000;

    indexHealthInfo.getLatestHealthInfo(this->index->_getNumberOfDocumentsInIndex());
    return returnValue;
}

void * dispatchMergeThread(void *indexer) {
	(reinterpret_cast <IndexReaderWriter *>(indexer))->startMergeThreadLoop();
	pthread_exit(0);
}

IndexReaderWriter::IndexReaderWriter(IndexMetaData* indexMetaData, Analyzer *analyzer, Schema *schema)
{
     // CREATE NEW Index
     this->index =  IndexData::create(indexMetaData->directoryName,
     		                          analyzer,
                                      schema,
                                      srch2::instantsearch::DISABLE_STEMMER_NORMALIZER
                                      );
     this->userFeedbackIndex = new FeedbackIndex(indexMetaData->maxFeedbackRecordsPerQuery,
    		 indexMetaData->maxCountOfFeedbackQueries, this);
     this->initIndexReaderWriter(indexMetaData);
     // start merge threads after commit
 }

IndexReaderWriter::IndexReaderWriter(IndexMetaData* indexMetaData)
{
    // LOAD Index
    this->index = IndexData::load(indexMetaData->directoryName);
    this->userFeedbackIndex = new FeedbackIndex(indexMetaData->maxFeedbackRecordsPerQuery,
    		indexMetaData->maxCountOfFeedbackQueries, this);
    this->userFeedbackIndex->load(indexMetaData->directoryName);
    this->initIndexReaderWriter(indexMetaData);
}

void IndexReaderWriter::initIndexReaderWriter(IndexMetaData* indexMetaData)
 {
     this->cache = dynamic_cast<CacheManager*>(indexMetaData->cache);
     this->mergeEveryNSeconds = indexMetaData->mergeEveryNSeconds;
     this->mergeEveryMWrites = indexMetaData->mergeEveryMWrites;
     this->updateHistogramEveryPMerges = indexMetaData->updateHistogramEveryPMerges;
     this->updateHistogramEveryQWrites = indexMetaData->updateHistogramEveryQWrites;
     this->writesCounterForMerge = 0;
     this->mergeCounterForUpdatingHistogram = 0;
     this->needToSaveIndexes = false;
     this->indexDirectoryName = indexMetaData->directoryName;

     this->mergeThreadStarted = false; // No threads running
     //zero indicates that the lockForWriters is unset
     pthread_mutex_init(&lockForWriters, 0); 
 }

uint32_t IndexReaderWriter::getNumberOfDocumentsInIndex() const
{
    return this->index->_getNumberOfDocumentsInIndex();
}



pthread_t IndexReaderWriter::createAndStartMergeThreadLoop() {
	pthread_attr_init(&mergeThreadAttributes);
	pthread_attr_setdetachstate(&mergeThreadAttributes, PTHREAD_CREATE_JOINABLE);
	pthread_create(&mergerThread, &mergeThreadAttributes, dispatchMergeThread, this);
	pthread_attr_destroy(&mergeThreadAttributes);
	// wait till merge thread is actually ready.
	while(!mergeThreadStarted) {
		sleep(1);
	}
	return mergerThread;
}

/*
 *    Entry point of inverted list merge worker threads. Each worker thread waits for notification
 *    from the master merge thread when the merge lists are ready for processing.
 */
void * dispatchMergeWorkerThread(void *arg) {
	MergeWorkersThreadArgs *info = (MergeWorkersThreadArgs *) arg;
	IndexData * index = (IndexData *)info->index;
	pthread_mutex_lock(&info->perThreadMutex);
	// Atomically set the flag to True
	// Details: https://gcc.gnu.org/onlinedocs/gcc-4.1.2/gcc/Atomic-Builtins.html
	__sync_or_and_fetch(&info->workerReady, true); 

	//  Note: swap part is redundant ( There is no atomic compare only API from gcc)
	//  Details: https://gcc.gnu.org/onlinedocs/gcc-4.1.2/gcc/Atomic-Builtins.html
	while(!__sync_bool_compare_and_swap(&info->stopExecuting, true, true)) {
		pthread_cond_wait(&info->waitConditionVar, &info->perThreadMutex);
		if (__sync_bool_compare_and_swap(&info->stopExecuting, true, true))
			break;
		if (__sync_bool_compare_and_swap(&info->isDataReady, true, true)) {
			//Logger::console("Worker %d : Starting Merge of Inverted Lists", info->workerId);
			unsigned processedCount  = index->invertedIndex->workerMergeTask(index->rankerExpression,
						index->_getNumberOfDocumentsInIndex(), index->schemaInternal, index->trie);

			__sync_and_and_fetch(&info->isDataReady, false); // set the flag to false atomically.

			// acquire the lock to make sure that main merge thread is waiting for this condition.
			// When the main thread is waiting on the condition then this lock is in unlocked state
			// and can be acquired.
			// if the lock is ignored then the condition signal sent by this thread could be
			// lost because main thread may be processing condition signal of the other thread.

			pthread_mutex_lock(&index->invertedIndex->dispatcherMutex);
			pthread_cond_signal(&index->invertedIndex->dispatcherConditionVar);
			pthread_mutex_unlock(&index->invertedIndex->dispatcherMutex);
			//Logger::console("Worker %d : Done with merge, processed %d list ", info->workerId, processedCount);
		} 	
	}

	pthread_mutex_unlock(&info->perThreadMutex);
	return NULL;
}
/*
 *    The API creates inverted list merge worker threads and initialize them.
 */
void IndexReaderWriter::createAndStartMergeWorkerThreads() {

	this->index->invertedIndex->mergeWorkersCount = 5;  // ToDo: make configurable later.

	unsigned mergeWorkersCount = this->index->invertedIndex->mergeWorkersCount;
	mergerWorkerThreads = new pthread_t[mergeWorkersCount];
	this->index->invertedIndex->mergeWorkersArgs = new MergeWorkersThreadArgs[mergeWorkersCount];
	MergeWorkersThreadArgs *mergeWorkersArgs = this->index->invertedIndex->mergeWorkersArgs;
	for (unsigned i = 0; i < mergeWorkersCount; ++i) {
		mergeWorkersArgs[i].index = this->index;
		mergeWorkersArgs[i].isDataReady = false;
		mergeWorkersArgs[i].stopExecuting = false;
		mergeWorkersArgs[i].workerId = i;
		mergeWorkersArgs[i].workerReady = false;
		pthread_mutex_init(&mergeWorkersArgs[i].perThreadMutex, NULL);
		pthread_cond_init(&mergeWorkersArgs[i].waitConditionVar, NULL);
		pthread_create(&mergerWorkerThreads[i], NULL,
				dispatchMergeWorkerThread, &mergeWorkersArgs[i]);
		// Logger::console("created merge worker thread %d", i);
	}

	// make sure all worker threads are ready before returning.
	bool allReady = false;
	while (!allReady) {
		allReady = true;
		for (unsigned i = 0; i < mergeWorkersCount; ++i) {
			if (__sync_bool_compare_and_swap(&mergeWorkersArgs[i].workerReady, false, false)) {
				allReady = false;
				sleep(1);
				break;
			}
		}
	}
}

//http://publib.boulder.ibm.com/infocenter/iseries/v5r4/index.jsp?topic=%2Fapis%2Fusers_77.htm
void IndexReaderWriter::startMergeThreadLoop()
{
    int               rc;
    struct timespec   ts;
    struct timeval    tp;

    /*
     *  There should be only one merger thread per indexer object
     */
    pthread_mutex_lock(&lockForWriters);
    if (mergeThreadStarted) {
      pthread_mutex_unlock(&lockForWriters);
    	Logger::warn("Only one merge thread per index is supported!");
    	return;
    }
    if (!this->index->isBulkLoadDone()) {
      pthread_mutex_unlock(&lockForWriters);
      Logger::warn("Merge thread can be called only after first commit!");
      return;
    }

    /*
     *  Initialize condition variable for the first time before loop starts.
     */
    pthread_cond_init(&countThresholdConditionVariable, NULL);
    mergeThreadStarted = true;
    while (1)
    {
        rc =  gettimeofday(&tp, NULL);
        // Convert from timeval to timespec
        ts.tv_sec  = tp.tv_sec;
        ts.tv_nsec = tp.tv_usec * 1000;
        ts.tv_sec += this->mergeEveryNSeconds;

        // lockForWriters mutex is unlocked inside pthread_cond_timedwait function and
        // acquired again before returning. We do not need to explicitly lock/unlock the mutex.
        rc = pthread_cond_timedwait(&countThresholdConditionVariable,
            &lockForWriters, &ts);

        if (mergeThreadStarted == false)
            break;
        else
        {
            this->doMerge();
        }
    }
    pthread_cond_destroy(&countThresholdConditionVariable);


    pthread_mutex_unlock(&lockForWriters);
    return;
}

void IndexReaderWriter::doMerge()
{
    // check to see if it's the time to update histogram information
    // if so, reset the merge counter for future.
    bool updateHistogramFlag = shouldUpdateHistogram();
    if(updateHistogramFlag == true){
        this->resetMergeCounterForHistogram();
    }
    this->merge(updateHistogramFlag);
    writesCounterForMerge = 0;
}

}
}


