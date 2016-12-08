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
 */

#include "index/InvertedIndex.h"
#include "index/Trie.h"
#include "util/Assert.h"
#include "util/Logger.h"
#include <math.h>

#include <algorithm>
#include <vector>
#include <iostream>

#include <cassert>

using std::endl;
using std::vector;
using srch2::util::Logger;

namespace srch2
{
namespace instantsearch
{
void InvertedListContainer::sortAndMergeBeforeCommit(const unsigned keywordId, const ForwardIndex *forwardIndex, bool needToSortEachInvertedList)
{
    // sort this inverted list only if the flag is true.
    // if the flag is false, we only need to commit.
    if (needToSortEachInvertedList) {
        shared_ptr<vectorview<ForwardListPtr> > forwardListDirectoryReadView;
        forwardIndex->getForwardListDirectory_ReadView(forwardListDirectoryReadView);

        vectorview<unsigned>* &writeView = this->invList->getWriteView();

        vector<InvertedListIdAndScore> invertedListElements(writeView->size());
        for (unsigned i = 0; i< writeView->size(); i++) {
            invertedListElements[i].recordId = writeView->getElement(i);
            invertedListElements[i].score = forwardIndex->getTermRecordStaticScore(invertedListElements[i].recordId,
            		forwardIndex->getKeywordOffset(forwardListDirectoryReadView, invertedListElements[i].recordId, keywordId));
        }
        std::sort(invertedListElements.begin(), invertedListElements.end(), InvertedListContainer::InvertedListElementGreaterThan());

        for (unsigned i = 0; i< writeView->size(); i++) {
            writeView->at(i) = invertedListElements[i].recordId;
        }
    }

    this->invList->commit();
}

// Main idea:
// 1) go through the elements in the write view. For each element,
// check if it's a valid record (i.e., not deleted in the forward index).
// 2) Count such valid records from the read view and write view separately;
// assuming those from the read view are already sorted based on scores.
// 3) Store these valid records into a temporary vector with their scores.
// Sort them assuming those from the read view are already sorted.
// 4) Copy them back to the write view, which is resized based on the
// total number of valid records.
//
// Return: number of elements in the final write view
int InvertedListContainer::sortAndMerge(const unsigned keywordId, ForwardIndex *forwardIndex,
		shared_ptr<vectorview<ForwardListPtr> >& forwardListDirectoryReadView,
	    vector<InvertedListIdAndScore>& invertedListElements,
        unsigned totalNumberOfDocuments,
		RankerExpression *rankerExpression, const Schema *schema)
{

    shared_ptr<vectorview<unsigned> > readView;
    this->invList->getReadView(readView);
    unsigned readViewListSize = readView->size();

    vectorview<unsigned>* &writeView = this->invList->getWriteView();
    unsigned writeViewListSize = writeView->size();

    // when merging an inverted list, we want to ignore those records that have
    // been deleted.  That info needs to be retrieved from the forward index.
    Logger::debug("SortnMerge: | %d | %d ", readViewListSize, writeViewListSize);
    ASSERT(readViewListSize <= writeViewListSize);

    bool isNewInvertedList = false;
    /*
     * if the readview and writeview are same. Then the keyword and this inverted list are new.
     */
    if (readView.get() == writeView) {
    	isNewInvertedList = true;
    }

    if (invertedListElements.capacity() < writeViewListSize)
    	invertedListElements.reserve(writeViewListSize);

    // copy the elements from the write view to a vector to sort
    // OPT: avoid this copy
    unsigned validRecordCountFromReadView = 0; // count # of records that are not deleted
    unsigned validRecordCountFromWriteView = 0; // count # of records that are not deleted

    for (unsigned invListIter = 0; invListIter < writeView->size(); invListIter++) {
        unsigned recordId = writeView->getElement(invListIter);

        bool valid = false;
        const ForwardList* forwardList = forwardIndex->getForwardList(forwardListDirectoryReadView,
               recordId, valid);
        // if the record is not valid (e.g., marked deleted), we ignore it
        if (!valid)
            continue;

        float idf = Ranker::computeIdf(totalNumberOfDocuments, writeViewListSize);
        unsigned recordLength = forwardList->getNumberOfKeywords();

        /*
         * Find the keyword offset using binary search on keyword ids. If the record is an
         * existing record then the keyword MUST be found and the keyword offset MUST be less than
         * the number of keywords in the record. For a newly added record (which is being merged
         * in the current merge cycle), it is possible that the keyword is not found by the
         * binary search because keyword ids may not be sorted yet since the record has a keyword
         * with a temporary, large ID that needs to be re-assigned later. In such a case, do
         * linear scan of keyword ids to find the keyword offset.
         */
        unsigned keywordOffset =  forwardList->getKeywordOffset(keywordId);
        if (keywordOffset >= recordLength){
        	if (!isNewInvertedList){
        		/*
        		 * if readview and writeview are not the same then ASSERT that the record is in
        		 * a write view. (i.e. it is a new record appended to the end of the inverted list)
        		 */
        		ASSERT(invListIter >= readViewListSize);
        	}
        	// scan the keyword-id list and get keyword offset.
        	keywordOffset = forwardList->getKeywordOffsetByLinearScan(keywordId);
        	ASSERT(keywordOffset < recordLength);
        }
        float recordBoost = forwardList->getRecordBoost();
        float tfBoostProduct = ((ForwardList*)forwardList)->getKeywordTfBoostProduct(keywordOffset);
        float textRelevance =  Ranker::computeTextRelevance(tfBoostProduct, idf);
        float score = rankerExpression->applyExpression(recordLength, recordBoost, textRelevance);
        ((ForwardList*)forwardList)->setKeywordRecordStaticScore(keywordOffset, score);
        // add this new <recordId, score> pair to the vector
        InvertedListIdAndScore iliasEntry = {recordId, score};
        invertedListElements.push_back(iliasEntry);
        if (!isNewInvertedList && invListIter < readViewListSize) {
        	/*
        	 * increment this counter only if the readview and the writeview are different. If they
        	 * are same then this is a new inverted list and all records are in write view.
        	 */
           validRecordCountFromReadView ++; // count the # of valid records
        }
        else
           validRecordCountFromWriteView ++;
    }

    unsigned newTotalSize = invertedListElements.size();
    std::sort(invertedListElements.begin() + validRecordCountFromReadView,
              invertedListElements.end(),
              InvertedListContainer::InvertedListElementGreaterThan());

    // if the read view and the write view are the same, it means we have added a new keyword with a new COWvector.
    // In this case, instead of calling "merge()", we call "commit()" to let this COWvector commit.
    if (readView.get() == writeView) {
        this->invList->commit();
        return this->invList->getWriteView()->size();
    }

    std::inplace_merge (invertedListElements.begin(),
            invertedListElements.begin() + validRecordCountFromReadView,
            invertedListElements.end(),
            InvertedListContainer::InvertedListElementGreaterThan());

    // If the read view and write view are sharing the same array, we have to separate the write view from the read view.
    if (writeView->getArray() == readView->getArray())
        writeView->forceCreateCopy();

    // OPT: resize the vector to shrink it if needed
    writeView->setSize(newTotalSize);
    for (unsigned i = 0; i < writeView->size(); i++) {
        writeView->at(i) = invertedListElements[i].recordId;
    }

    this->invList->merge();
    return this->invList->getWriteView()->size();
}


InvertedIndex::InvertedIndex(ForwardIndex *forwardIndex)
{
    //this->invertedIndexVector = new cowvector<InvertedListContainerPtr>(100);
    this->invertedIndexVector = NULL;
    this->keywordIds = NULL;
    this->forwardIndex = forwardIndex;
    this->commited_WriteView = false;
    pthread_mutex_init(&dispatcherMutex, NULL);
    pthread_cond_init(&dispatcherConditionVar, NULL);
    mergeWorkersCount = 0;
}

InvertedIndex::~InvertedIndex()
{
    if (this->invertedIndexVector != NULL) {
        if (this->commited_WriteView) {
            this->invertedIndexVector->merge();
            this->keywordIds->merge();
        } else {
            this->invertedIndexVector->commit();
            this->keywordIds->commit();
        }
        for (unsigned invertedIndexIter = 0; invertedIndexIter < this->getTotalNumberOfInvertedLists_ReadView(); ++invertedIndexIter) {
            delete this->invertedIndexVector->getWriteView()->getElement(invertedIndexIter);
        }
        this->invertedListSizeDirectory.clear();
        delete this->invertedIndexVector;
        delete this->keywordIds;
    }
}

bool InvertedIndex::isValidTermPositionHit(shared_ptr<vectorview<ForwardListPtr> > & forwardIndexDirectoryReadView,
		unsigned forwardListId,
		unsigned keywordOffset,
        const vector<unsigned>& filterAttributesList, ATTRIBUTES_OP attrOp,
        vector<unsigned>& matchingKeywordAttributesList, float &termRecordStaticScore) const
{
    return this->forwardIndex->isValidRecordTermHit(forwardIndexDirectoryReadView, forwardListId, keywordOffset,
    		filterAttributesList, attrOp, matchingKeywordAttributesList, termRecordStaticScore);
}

// given a forworListId and invertedList offset, return the keyword offset
unsigned InvertedIndex::getKeywordOffset(shared_ptr<vectorview<ForwardListPtr> > & forwardListDirectoryReadView,
		shared_ptr<vectorview<unsigned> > & invertedIndexKeywordIdsReadView,
		unsigned forwardListId, unsigned invertedListOffset) const
{
    //transfer the invertedList offset to keywordId
    return this->forwardIndex->getKeywordOffset(forwardListDirectoryReadView, forwardListId, invertedIndexKeywordIdsReadView->getElement(invertedListOffset));
}

// For Trie bootstrap
void InvertedIndex::incrementDummyHitCount(unsigned invertedIndexDirectoryIndex)
{
    if ( this->commited_WriteView == false) {
        if ( invertedIndexDirectoryIndex == this->invertedListSizeDirectory.size() ) {
            this->invertedListSizeDirectory.push_back(0);
        } else  if (invertedIndexDirectoryIndex < this->invertedListSizeDirectory.size()) {
            // do nothing
        }
    }
}
void InvertedIndex::incrementHitCount(unsigned invertedIndexDirectoryIndex)
{
    if ( this->commited_WriteView == false) {
        if ( invertedIndexDirectoryIndex == this->invertedListSizeDirectory.size() ) {
            this->invertedListSizeDirectory.push_back(1);
        } else  if (invertedIndexDirectoryIndex < this->invertedListSizeDirectory.size()) {
            this->invertedListSizeDirectory.at(invertedIndexDirectoryIndex) += 1;
        }
    } else {
        vectorview<InvertedListContainerPtr>* &writeView = this->invertedIndexVector->getWriteView();
        if (invertedIndexDirectoryIndex == writeView->size()) {
        	InvertedListContainerPtr newInvertedListAfterBulkLoad = new InvertedListContainer(1);
        	// This block is executed after bulkload. commit the new list to separate the readview and the writeview.
        	newInvertedListAfterBulkLoad->invList->commit();
            writeView->push_back(newInvertedListAfterBulkLoad);
        }
    }
}

float InvertedIndex::getIdf(const unsigned totalNumberOfDocuments, const unsigned keywordId) const
{
    float idf = 0.0;
    if ( this->commited_WriteView == false) {
        ASSERT(keywordId < this->invertedListSizeDirectory.size());
        idf = Ranker::computeIdf(totalNumberOfDocuments, this->invertedListSizeDirectory.at(keywordId));
    } else {
        vectorview<InvertedListContainerPtr>* &writeView = this->invertedIndexVector->getWriteView();

        ASSERT(keywordId < writeView->size());

        idf = Ranker::computeIdf(totalNumberOfDocuments, writeView->getElement(keywordId)->getWriteViewSize());
    }
    return idf;
}

float InvertedIndex::computeRecordStaticScore(RankerExpression *rankerExpression, const float recordBoost,
        const float recordLength, const float idf,
        const float tfBoostProduct) const
{
    // recordScoreType == srch2::instantsearch::LUCENESCORE:
    float textRelevance =  Ranker::computeTextRelevance(tfBoostProduct, idf);
    return rankerExpression->applyExpression(recordLength, recordBoost, textRelevance);
}

void InvertedIndex::initialiseInvertedIndexCommit()
{
    ASSERT(this->commited_WriteView != true); // TODO: can be removed for optimization.

    //delete this->invertedIndexVector;
    this->invertedIndexVector = new cowvector<InvertedListContainerPtr>(this->invertedListSizeDirectory.size());
    this->keywordIds = new cowvector<unsigned> (this->invertedListSizeDirectory.size());
    vectorview<InvertedListContainerPtr>* &writeView = this->invertedIndexVector->getWriteView();
    for (vector<unsigned>::iterator vectorIterator = this->invertedListSizeDirectory.begin();
            vectorIterator != this->invertedListSizeDirectory.end();
            vectorIterator++) {
        InvertedListContainerPtr invListContainerPtr;
        invListContainerPtr = new InvertedListContainer(*vectorIterator);
        writeView->push_back(invListContainerPtr);
    }
}
/* COMMIT LOGIC
 * 1. Get all the records from the forwardIndex
 * 2. For each record
 *         2.1 Get the forwardList corresponding to the record
 *         2.2 Get the recordBoost from the forwardList
 *         2.2 For each keyword in the forwardList
 *             2.2.1 Calculate the score
 *          2.2.2 Add the keyword,rid to the InvertedIndex
 *          2.2.3 Add the score in the forwardIndex
 */

void InvertedIndex::commit( ForwardList *forwardList,
                            RankerExpression *rankerExpression,
                            const unsigned forwardListOffset, const unsigned totalNumberOfDocuments,
                            const Schema *schema, const vector<NewKeywordIdKeywordOffsetTriple> &newKeywordIdKeywordOffsetTriple)
{
    if (this->commited_WriteView == false) {
        //unsigned sumOfOccurancesOfAllKeywordsInRecord = 0;
        float recordBoost = forwardList->getRecordBoost();

        for (unsigned counter = 0; counter < forwardList->getNumberOfKeywords(); counter++) {
            //unsigned keywordId = forwardIndex->getForwardListElementByDirectory(forwardListOffset , counter);//->keywordId;
            unsigned keywordId = newKeywordIdKeywordOffsetTriple.at(counter).first;
            unsigned invertedListId = newKeywordIdKeywordOffsetTriple.at(counter).second.second;
            float idf = this->getIdf(totalNumberOfDocuments, invertedListId); // Uses invertedListSizeDirectory at commit stage

            ///Use a positionIndexDirectory to get the size of records for calculating tf
            //unsigned numberOfOccurancesOfGivenKeywordInRecord = forwardList->getNumberOfPositionHitsForAllKeywords(schema);
            //sumOfOccurancesOfAllKeywordsInRecord += numberOfOccurancesOfGivenKeywordInRecord;

            float tfBoostProduct = forwardList->getKeywordTfBoostProduct(counter);
            float recordLength = forwardList->getNumberOfKeywords();
            float score = this->computeRecordStaticScore(rankerExpression, recordBoost, recordLength, idf, tfBoostProduct);

            //assign keywordId for the invertedListId
            vectorview<unsigned>* &writeView = this->keywordIds->getWriteView();
            writeView->at(invertedListId) = keywordId;
            this->addInvertedListElement(invertedListId, forwardListOffset);
            forwardList->setKeywordTfBoostProduct(counter, tfBoostProduct);
            forwardList->setKeywordRecordStaticScore(counter, score);
        }
    }
}

void InvertedIndex::finalCommit(bool needToSortEachInvertedList)
{
    vectorview<InvertedListContainerPtr>* &writeView = this->invertedIndexVector->getWriteView();
    unsigned sizeOfList = writeView->size();
    vectorview<unsigned>* &keywordIdsWriteView = this->keywordIds->getWriteView();

    for (unsigned iter = 0; iter < sizeOfList; ++iter) {
        writeView->at(iter)->sortAndMergeBeforeCommit(keywordIdsWriteView->getElement(iter), this->forwardIndex, needToSortEachInvertedList);
    }

    this->invertedIndexVector->commit();
    this->keywordIds->commit();
    this->commited_WriteView = true;
    this->invertedListSizeDirectory.clear();
}

void InvertedIndex::merge(RankerExpression *rankerExpression, unsigned totalNumberOfDocuments,
		const Schema *schema, Trie *trie)
{
    this->invertedIndexVector->merge(&invertedIndexVectorReadViewsMgr);
    this->keywordIds->merge(&keywordIdsReadViewsMgr);

    vectorview<InvertedListContainerPtr>* &writeView = this->invertedIndexVector->getWriteView();
    // get keywordIds writeView
    vectorview<unsigned>* &keywordIdsWriteView = this->keywordIds->getWriteView();

    shared_ptr<vectorview<ForwardListPtr> > forwardListDirectoryReadView;
    forwardIndex->getForwardListDirectory_ReadView(forwardListDirectoryReadView);
    vector<InvertedListIdAndScore> invertedListElements;
    for (set<pair<unsigned, unsigned> >::const_iterator iter = this->invertedListKeywordSetToMerge.begin();
        iter != this->invertedListKeywordSetToMerge.end(); ++iter) {
    	ASSERT(iter->first < writeView->size()); // iter->first is invertedListId
    	int finalInvListWriteViewSize =
             writeView->at(iter->first)->sortAndMerge(keywordIdsWriteView->getElement(iter->first),
    			this->forwardIndex, forwardListDirectoryReadView, invertedListElements,
    			totalNumberOfDocuments, rankerExpression, schema);
    	invertedListElements.clear();
    	if (finalInvListWriteViewSize == 0) {
            // This inverted list is empty, so we add it to the list
            // of empty leaf node ids to delete later
            trie->addEmptyLeafNodeId(iter->second); // add the keyword Id
    	}
    }
    this->invertedListKeywordSetToMerge.clear();
}

unsigned  InvertedIndex::workerMergeTask(RankerExpression *rankerExpression,
		unsigned totalNumberOfDocuments, const Schema *schema, Trie *trie) {
	unsigned totalListProcessed = 0;
	shared_ptr<vectorview<ForwardListPtr> > forwardListDirectoryReadView;
	forwardIndex->getForwardListDirectory_ReadView(forwardListDirectoryReadView);
	// get inverted list writeView
	vectorview<InvertedListContainerPtr>* &writeView = this->invertedIndexVector->getWriteView();
	// get keywordIds writeView
	vectorview<unsigned>* &keywordIdsWriteView = this->keywordIds->getWriteView();
	// this vector is used as a placeholder for sorting during the sortAndMerge.
	vector<InvertedListIdAndScore> invertedListElements;

	while (true){
		unsigned cursor ;

		// First fetch the cursor of the queue
		{
			boost::unique_lock<boost::mutex> Lock(mergeWorkersSharedQueue._lock);
			if (mergeWorkersSharedQueue.cursor < mergeWorkersSharedQueue.dataLen) {
				// if shared cursor is less than shared queue size, then cache the cursor locally
				// and increment the shared cursor.
				cursor = mergeWorkersSharedQueue.cursor++;
			} else {
				// else exit the loop because there are no more lists to process.
				break;
			}
		}

		++totalListProcessed;
		unsigned invertedListId = mergeWorkersSharedQueue.invertedListKeywordIds[cursor].first;
		unsigned keywordId = mergeWorkersSharedQueue.invertedListKeywordIds[cursor].second;
		ASSERT(invertedListId < writeView->size());
		if (invertedListId < writeView->size()) {
            int finalInvListWriteViewSize =
                    writeView->at(invertedListId)->sortAndMerge(keywordIdsWriteView->getElement(invertedListId),
                      this->forwardIndex,forwardListDirectoryReadView, invertedListElements,
                      totalNumberOfDocuments, rankerExpression, schema);
            invertedListElements.clear();

            if (finalInvListWriteViewSize == 0) {
	            // This inverted list is empty, so we add it to the list
	            // of empty leaf node ids to delete later
	            trie->addEmptyLeafNodeId(keywordId); // add the keyword Id
	    	}

		} else {
			Logger::info("Invalid list Id = %d for merge", invertedListId);
		}
	}
	return totalListProcessed;
}

void InvertedIndex::parallelMerge()
{
    this->invertedIndexVector->merge();
    this->keywordIds->merge();

    unsigned totalLoad = this->invertedListKeywordSetToMerge.size();

    if (totalLoad == 0)
    	return;

    // copy inverted list ids and keyword ids to an array from the set.
    pair<unsigned, unsigned> *workerIdsList = new pair<unsigned, unsigned>[totalLoad];
    unsigned i = 0;
    for (set<pair<unsigned, unsigned> >::const_iterator iter = this->invertedListKeywordSetToMerge.begin();
        iter != this->invertedListKeywordSetToMerge.end(); ++iter) {
    	workerIdsList[i++] = *iter; // <invertedListId, keywordId>
    }

    // initialize worker queue
    mergeWorkersSharedQueue.invertedListKeywordIds = workerIdsList;
    mergeWorkersSharedQueue.dataLen = totalLoad;
    mergeWorkersSharedQueue.cursor = 0;

    // Acquire mutex before notifying worker threads to start processing the task. It will avoid a
    // race condition which can occur if the worker threads finish executing before
    // this main thread starts waiting (using pthread_cond_wait) for the 'done' signal from workers.
    // All the workers will try to acquire this mutex before notifying the main thread about completion
    // of the task. The mutex is released by main thread only when it is actually waiting for
    // 'done' signal.
    pthread_mutex_lock(&dispatcherMutex);
    // Notify each worker that queue is ready.
    for (unsigned i = 0; i < mergeWorkersCount; ++i) {
    	pthread_mutex_lock(&mergeWorkersArgs[i].perThreadMutex);
    	__sync_or_and_fetch(&mergeWorkersArgs[i].isDataReady, true);
    	pthread_cond_signal(&mergeWorkersArgs[i].waitConditionVar);
    	pthread_mutex_unlock(&mergeWorkersArgs[i].perThreadMutex);
    }

    // wait for all workers to finish
    bool allDone;
    do{
    	// dispatcherMutex is released in this function and acquired again before returning.
    	pthread_cond_wait(&dispatcherConditionVar, &dispatcherMutex);
    	allDone = true;
    	for (unsigned i = 0; i < mergeWorkersCount; ++i) {
    		if (__sync_bool_compare_and_swap(&mergeWorkersArgs[i].isDataReady, true, true)) {
    			allDone = false;
    			break;
    		}
    	}
    }while(!allDone);
    pthread_mutex_unlock(&dispatcherMutex);

    // reset workers queue
    mergeWorkersSharedQueue.invertedListKeywordIds = NULL;
    mergeWorkersSharedQueue.dataLen = 0;
    mergeWorkersSharedQueue.cursor = 0;

    delete[] workerIdsList;
    this->invertedListKeywordSetToMerge.clear();
}

// recordInternalId is same as forwardIndeOffset
void InvertedIndex::addRecord(ForwardList* forwardList, Trie * trie,
                              RankerExpression *rankerExpression,
                              const unsigned forwardListOffset, const SchemaInternal *schema,
                              const Record *record, const unsigned totalNumberOfDocuments,
                              const KeywordIdKeywordStringInvertedListIdTriple &keywordIdList)
{
    if (this->commited_WriteView == true) {
        //unsigned sumOfOccurancesOfAllKeywordsInRecord = 0;
        float recordBoost = record->getRecordBoost();


        for (unsigned counter = 0; counter < keywordIdList.size(); counter++) {
            unsigned keywordId = keywordIdList.at(counter).first;
            unsigned invertedListId = keywordIdList.at(counter).second.second;

            ///Use a positionIndexDirectory to get the size of  for calculating tf
            //unsigned numberOfOccurancesOfGivenKeywordInRecord = forwardList->getNumberOfPositionHitsForAllKeywords(schema);
            //sumOfOccurancesOfAllKeywordsInRecord += numberOfOccurancesOfGivenKeywordInRecord;

            float recordLength = forwardList->getNumberOfKeywords();

            vectorview<unsigned>* &writeView = this->keywordIds->getWriteView();

            writeView->at(invertedListId) = keywordId;
            this->addInvertedListElement(invertedListId, forwardListOffset);
            this->invertedListKeywordSetToMerge.insert(make_pair(invertedListId, keywordId));

            float idf = this->getIdf(totalNumberOfDocuments, invertedListId);
            float tfBoostProduct = forwardList->getKeywordTfBoostProduct(counter);
            float score = this->computeRecordStaticScore(rankerExpression, recordBoost, recordLength, idf, tfBoostProduct);

            // now we should update the trie by this score
            trie->updateMaximumScoreOfLeafNodesForKeyword_WriteView(keywordId , (half)score);
            // and update the scores in forward index
            forwardList->setKeywordTfBoostProduct(counter, tfBoostProduct);
            forwardList->setKeywordRecordStaticScore(counter, score);
        }
    }
}


void InvertedIndex::addInvertedListElement(unsigned keywordId, unsigned recordId)
{
    vectorview<InvertedListContainerPtr>* &writeView = this->invertedIndexVector->getWriteView();

    ASSERT( keywordId < writeView->size());

    writeView->at(keywordId)->addInvertedListElement(recordId);
}

void InvertedIndex::getInvertedListReadView(shared_ptr<vectorview<InvertedListContainerPtr> > & invertedListDirectoryReadView,
		const unsigned invertedListId, shared_ptr<vectorview<unsigned> >& invertedListReadView) const
{
    ASSERT(invertedListId < this->getTotalNumberOfInvertedLists_ReadView());
    invertedListDirectoryReadView->getElement(invertedListId)->getInvertedList(invertedListReadView);
}

void InvertedIndex::getInvertedIndexDirectory_ReadView(
		shared_ptr<vectorview<InvertedListContainerPtr> > & invertedListDirectoryReadView) const{
	this->invertedIndexVector->getReadView(invertedListDirectoryReadView);
}

void InvertedIndex::getInvertedIndexKeywordIds_ReadView(shared_ptr<vectorview<unsigned> > & invertedIndexKeywordIdsReadView) const{
	this->keywordIds->getReadView(invertedIndexKeywordIdsReadView);
}

//ReadView InvertedListSize
unsigned InvertedIndex::getInvertedListSize_ReadView(const unsigned invertedListId) const
{
    // A valid list ID is in the range [0, 1, ..., directorySize - 1]
    if (invertedListId >= this->getTotalNumberOfInvertedLists_ReadView() )
        return 0;

    shared_ptr<vectorview<InvertedListContainerPtr> > readView;
    this->invertedIndexVector->getReadView(readView);
    return readView->getElement(invertedListId)->getReadViewSize();
}

unsigned InvertedIndex::getRecordNumber() const
{
    return this->forwardIndex->getTotalNumberOfForwardLists_ReadView();
}

unsigned InvertedIndex::getTotalNumberOfInvertedLists_ReadView() const
{
    shared_ptr<vectorview<InvertedListContainerPtr> > readView;
    this->invertedIndexVector->getReadView(readView);
    return readView->size();
}

int InvertedIndex::getNumberOfBytes() const
{

    //return (sizeof(InvertedListElement) * this->totalSizeOfInvertedIndex) + sizeof(invertedIndexVector) + sizeof(this->totalSizeOfInvertedIndex) + sizeof(this->forwardIndex);
    return ~0;
}

void InvertedIndex::print_test() const
{
    Logger::debug("InvertedIndex is:");
    shared_ptr<vectorview<InvertedListContainerPtr> > readView;
    this->invertedIndexVector->getReadView(readView);
    shared_ptr<vectorview<unsigned> > keywordIdsReadView;
    this->keywordIds->getReadView(keywordIdsReadView);
    for (unsigned vectorIterator =0;
            vectorIterator != readView->size();
            vectorIterator++) {
        Logger::debug("Inverted List: %d, KeywordId: %d", vectorIterator , keywordIdsReadView->at(vectorIterator));
        this->printInvList(vectorIterator);
    }
}
/*
 *   This API appends the inverted lists supplied as an input to a set of inverted list ids and
 *   their keyword ids that need to be merged.
 */
void InvertedIndex::appendInvertedListKeywordIdsForMerge(const vector<pair<unsigned, unsigned> >& invertedListKeywordIds){
    for (unsigned i = 0; i < invertedListKeywordIds.size(); ++i) {
       invertedListKeywordSetToMerge.insert(invertedListKeywordIds[i]);
    }
 }


}
}

