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

#include <instantsearch/Ranker.h>
#include "TermVirtualList.h"
#include "util/Assert.h"
#include "util/Logger.h"
#include "index/Trie.h"
#include "index/InvertedIndex.h"

using srch2::util::Logger;
namespace srch2
{
namespace instantsearch
{

void TermVirtualList::initialiseTermVirtualListElement(TrieNodePointer prefixNode,
        TrieNodePointer leafNode, unsigned distance)
{
    unsigned invertedListId = leafNode->getInvertedListOffset();
    unsigned invertedListCounter = 0;

    shared_ptr<vectorview<unsigned> > invertedListReadView;
    this->invertedIndex->getInvertedListReadView(this->invertedListDirectoryReadView,
    		invertedListId, invertedListReadView);
    unsigned recordId = invertedListReadView->getElement(invertedListCounter);
    // calculate record offset online
    unsigned keywordOffset = this->invertedIndex->getKeywordOffset(this->forwardIndexDirectoryReadView,
    		this->invertedIndexKeywordIdsReadView,
    		recordId, invertedListId);
    ++ invertedListCounter;

    bool foundValidHit = 0;
    float termRecordStaticScore = 0;
    vector<unsigned> matchedAttributeIdsList;
    while (1) {
        // We ignore the record if it's no longer valid
        if (keywordOffset != FORWARDLIST_NOTVALID &&
            this->invertedIndex->isValidTermPositionHit(this->forwardIndexDirectoryReadView,
                recordId,
                keywordOffset,
                term->getAttributesToFilter(), term->getFilterAttrOperation(), matchedAttributeIdsList,
                termRecordStaticScore) ) {
            foundValidHit = 1;
            break;
        }

        if (invertedListCounter < invertedListReadView->size()) {
            recordId = invertedListReadView->getElement(invertedListCounter);
            // calculate record offset online
            keywordOffset = this->invertedIndex->getKeywordOffset(this->forwardIndexDirectoryReadView,
            		this->invertedIndexKeywordIdsReadView,
            		recordId, invertedListId);
            ++invertedListCounter;
        } else {
            break;
        }
    }

    if (keywordOffset != FORWARDLIST_NOTVALID && foundValidHit == 1) {
        this->numberOfItemsInPartialHeap ++; // increment partialHeap counter

        if (this->numberOfItemsInPartialHeap == 0)
            this->currentMaxEditDistanceOnHeap = distance;

        if (this->getTermType() == srch2::instantsearch::TERM_TYPE_PREFIX) { // prefix term
            bool isPrefixMatch = (prefixNode != leafNode);
            float termRecordRuntimeScore =
                DefaultTopKRanker::computeTermRecordRuntimeScore(termRecordStaticScore,
                        distance,
                        term->getKeyword()->size(),
                        isPrefixMatch,
                        this->prefixMatchPenalty , term->getSimilarityBoost());
            this->itemsHeap.push_back(new HeapItem(invertedListId, this->cursorVector.size(),
                                                   recordId, matchedAttributeIdsList, termRecordRuntimeScore,
                                                   keywordOffset, prefixNode,
                                                   distance, isPrefixMatch));
        } else { // complete term
            float termRecordRuntimeScore =
                DefaultTopKRanker::computeTermRecordRuntimeScore(termRecordStaticScore,
                        distance,
                        term->getKeyword()->size(),
                        false,
                        this->prefixMatchPenalty , term->getSimilarityBoost());// prefix match == false
            this->itemsHeap.push_back(new HeapItem(invertedListId, this->cursorVector.size(),
                                                   recordId, matchedAttributeIdsList, termRecordRuntimeScore,
                                                   keywordOffset, leafNode, distance, false));
        }

        // Cursor points to the next element on InvertedList
        this->cursorVector.push_back(invertedListCounter);
        // keep the inverted list readviews in invertedListVector such that we can safely access them
        this->invertedListReadViewVector.push_back(invertedListReadView);
    }
}

void TermVirtualList::depthInitializeTermVirtualListElement(const TrieNode* trieNode, unsigned editDistance,
		unsigned panDistance, unsigned bound)
{
    if (trieNode->isTerminalNode()) {
        initialiseTermVirtualListElement(NULL, trieNode, editDistance > panDistance ? editDistance : panDistance);
    }

    if (panDistance < bound) {
        for (unsigned int childIterator = 0; childIterator < trieNode->getChildrenCount(); childIterator++) {
            const TrieNode *child = trieNode->getChild(childIterator);
            depthInitializeTermVirtualListElement(child, editDistance, panDistance + 1, bound);
        }
    }
}

// this function will depth initialize bitset, which will be used for complete term
void TermVirtualList::depthInitializeBitSet(const TrieNode* trieNode, unsigned editDistance, unsigned panDistance, unsigned bound)
{
    if (trieNode->isTerminalNode()) {
        unsigned invertedListId = trieNode->getInvertedListOffset();
        this->invertedIndex->getInvertedListReadView(this->invertedListDirectoryReadView,
        		invertedListId, invertedListReadView);
        for (unsigned invertedListCounter = 0; invertedListCounter < invertedListReadView->size(); invertedListCounter++) {
            // set the bit of the record id to be true
            if (!bitSet.getAndSet(invertedListReadView->at(invertedListCounter)))
                bitSetSize++;
        }
    }
    if (panDistance < bound) {
        for (unsigned int childIterator = 0; childIterator < trieNode->getChildrenCount(); childIterator++) {
            const TrieNode *child = trieNode->getChild(childIterator);
            // As we visit the children of the current trie node, we use "panDistance + 1" as the new
            // edit distance and ignore the "edit distance" value passed from the caller by using "0" and "false".
            depthInitializeBitSet(child,  editDistance, panDistance + 1, bound);
        }
    }
}

// Iterate over active nodes, fill the vector, and call make_heap on it.
TermVirtualList::TermVirtualList(const InvertedIndex* invertedIndex,  const ForwardIndex * forwardIndex, PrefixActiveNodeSet *prefixActiveNodeSet,
                                 Term *term, float prefixMatchPenalty, float shouldIterateToLeafNodesAndScoreOfTopRecord )
{
    this->invertedIndex = invertedIndex;
    this->invertedIndex->getInvertedIndexDirectory_ReadView(this->invertedListDirectoryReadView);
    this->invertedIndex->getInvertedIndexKeywordIds_ReadView(this->invertedIndexKeywordIdsReadView);
    forwardIndex->getForwardListDirectory_ReadView(this->forwardIndexDirectoryReadView);
    this->prefixActiveNodeSet = prefixActiveNodeSet;
    this->term = term;
    this->prefixMatchPenalty = prefixMatchPenalty;
    this->numberOfItemsInPartialHeap = 0;
    this->currentMaxEditDistanceOnHeap = 0;
    this->currentRecordID = -1;
    this->usingBitset = false;
    this->bitSetSize = 0;
    this->maxScoreForBitSetCase = 0;
    // this flag indicates whether this TVL is for a tooPopular term or not.
    // If it is a TVL of a too popular term, this TVL is disabled, meaning it should not be used for iteration over
    // heapItems. In this case shouldIterateToLeafNodesAndScoreOfTopRecord is not equal to -1
    this->topRecordScoreWhenListIsDisabled = shouldIterateToLeafNodesAndScoreOfTopRecord;
    if(this->topRecordScoreWhenListIsDisabled != -1){
    	return;
    }
    // check the TermType
    if (this->getTermType() == TERM_TYPE_PREFIX) { //case 1: Term is prefix
        LeafNodeSetIteratorForPrefix iter(prefixActiveNodeSet, term->getThreshold());
        // This is our query-optimization logic. If the total number of leaf nodes for the term is
        // greater than a threshold, we use bitset to do the union/intersection operations.
        if (iter.size() >= TERM_COUNT_THRESHOLD){
        	// NOTE: BitSet is disabled for now since the fact that it must always return max score of leaf nodes
        	// disables early termination and even makes the engine slower.
//            this->usingBitset = true;
        	this->usingBitset = false;
        }
        if (this->usingBitset) {// This term needs bitset
            bitSet.resize(this->invertedIndex->getRecordNumber());
            TrieNodePointer leafNode;
            TrieNodePointer prefixNode;
            unsigned distance;
            //numberOfLeafNodes = 0;
            //totalInveretListLength  = 0;
            this->maxScoreForBitSetCase = 0;
            for (; !iter.isDone(); iter.next()) {
                iter.getItem(prefixNode, leafNode, distance);
                float runTimeScoreOfThisLeafNode = DefaultTopKRanker::computeTermRecordRuntimeScore(leafNode->getMaximumScoreOfLeafNodes(),
    					distance,
    					term->getKeyword()->size(),
    					true,
    					this->prefixMatchPenalty , term->getSimilarityBoost());
                if( runTimeScoreOfThisLeafNode > this->maxScoreForBitSetCase){
                	this->maxScoreForBitSetCase = runTimeScoreOfThisLeafNode;
                }
                unsigned invertedListId = leafNode->getInvertedListOffset();
                this->invertedIndex->getInvertedListReadView(this->invertedListDirectoryReadView,
                		invertedListId, invertedListReadView);
                // loop the inverted list to add it to the Bitset
                for (unsigned invertedListCounter = 0; invertedListCounter < invertedListReadView->size(); invertedListCounter++) {
                    // We compute the union of these bitsets. We increment the number of bits only if the previous bit was 0.
                    if (!bitSet.getAndSet(invertedListReadView->at(invertedListCounter)))
                        bitSetSize ++;
                }
                //termCount++;
                //totalInveretListLength  += invertedListReadView->size();
            }

            bitSetIter = bitSet.iterator();
        } else { // If we don't use a bitset, we use the TA algorithm
            cursorVector.reserve(iter.size());
            invertedListReadViewVector.reserve(iter.size());
            for (; !iter.isDone(); iter.next()) {
                TrieNodePointer leafNode;
                TrieNodePointer prefixNode;
                unsigned distance;
                iter.getItem(prefixNode, leafNode, distance);
                initialiseTermVirtualListElement(prefixNode, leafNode, distance);
            }
        }
    } else { // case 2: Term is complete
        ActiveNodeSetIterator iter(prefixActiveNodeSet, term->getThreshold());
        if (iter.size() >= TERM_COUNT_THRESHOLD){
        	// NOTE: BitSet is disabled for now since the fact that it must always return max score of leaf nodes
        	// disables early termination and even makes the engine slower.
//            this->usingBitset = true;
        	this->usingBitset = false;
        }
        if (this->usingBitset) { // if this term virtual list is merged to a Bitset
            bitSet.resize(this->invertedIndex->getRecordNumber());
            TrieNodePointer trieNode;
            unsigned editDistance;
            //numberOfLeafNodes = 0;
            //totalInveretListLength  = 0;
            this->maxScoreForBitSetCase = 0;
            for (; !iter.isDone(); iter.next()) {
                iter.getItem(trieNode, editDistance);
                float runTimeScoreOfThisLeafNode = DefaultTopKRanker::computeTermRecordRuntimeScore(trieNode->getMaximumScoreOfLeafNodes(),
    					editDistance,
    					term->getKeyword()->size(),
    					false,
    					this->prefixMatchPenalty , term->getSimilarityBoost());
                if( runTimeScoreOfThisLeafNode > this->maxScoreForBitSetCase){
                	this->maxScoreForBitSetCase = runTimeScoreOfThisLeafNode;
                }

                unsigned panDistance = prefixActiveNodeSet->getEditdistanceofPrefix(trieNode);
                // we always use the panDistance between this pivotal active node (PAN) and the query term
                depthInitializeBitSet(trieNode, editDistance, panDistance, term->getThreshold());
            }

            bitSetIter = bitSet.iterator();
        } else {
            cursorVector.reserve(iter.size());
            invertedListReadViewVector.reserve(iter.size());
            for (; !iter.isDone(); iter.next()) {
                TrieNodePointer trieNode;
                unsigned editDistance;
                iter.getItem(trieNode, editDistance);
                unsigned panDistance = prefixActiveNodeSet->getEditdistanceofPrefix(trieNode);

                depthInitializeTermVirtualListElement(trieNode, editDistance, panDistance, term->getThreshold());
            }
        }
    }

    // Make partial heap by calling make_heap from begin() to begin()+"number of items within edit distance threshold"
    make_heap(itemsHeap.begin(), itemsHeap.begin()+numberOfItemsInPartialHeap, TermVirtualList::HeapItemCmp());
}
bool TermVirtualList::isTermVirtualListDisabled(){
	return (this->topRecordScoreWhenListIsDisabled != -1);
}

float TermVirtualList::getScoreOfTopRecordWhenListIsDisabled(){
	ASSERT(this->topRecordScoreWhenListIsDisabled != -1);
	return this->topRecordScoreWhenListIsDisabled;
}

TermVirtualList::~TermVirtualList()
{
    for (vector<HeapItem* >::iterator iter = this->itemsHeap.begin(); iter != this->itemsHeap.end(); iter++) {
        HeapItem *currentItem = *iter;
        if (currentItem != NULL)
            delete currentItem;
    }
    this->itemsHeap.clear();
    this->cursorVector.clear();
    this->invertedListReadViewVector.clear();
    this->term = NULL;
    this->invertedIndex = NULL;

}
//Called when this->numberOfItemsInPartialHeap = 0
bool TermVirtualList::_addItemsToPartialHeap()
{
    bool returnValue = false;
    // If partial heap is empty, increase editDistanceThreshold and add more elements
    for ( vector<HeapItem* >::iterator iter = this->itemsHeap.begin(); iter != this->itemsHeap.end(); iter++) {
        HeapItem *currentItem = *iter;
        if (this->numberOfItemsInPartialHeap == 0) {
            // partialHeap is empty, assign new maxEditDistance and add items to partialHeap
            if (currentItem->ed > this->currentMaxEditDistanceOnHeap) {
                this->currentMaxEditDistanceOnHeap = currentItem->ed;
                this->numberOfItemsInPartialHeap++;
                returnValue =true;
            }
        } else {
            // Edit distance on itemHeap is less than maxEditDistance so far seen
            if (currentItem->ed <= this->currentMaxEditDistanceOnHeap) {
                this->numberOfItemsInPartialHeap++;
                returnValue = true;
            }        //stopping condition: partialheap is not empty and edit distance is greater than maxEditDistance
            else
                break;
        }
    }
    // PartialHeap changed;
    if (returnValue) {
        make_heap(this->itemsHeap.begin(),this->itemsHeap.begin()+numberOfItemsInPartialHeap,
                  TermVirtualList::HeapItemCmp());
    }
    return returnValue;
}

bool TermVirtualList::getMaxScore(float & score)
{
    if (this->usingBitset) {
        score = this->maxScoreForBitSetCase;
        return true;
    } else {
        //If heapVector is empty
        if (this->itemsHeap.size() == 0) {
            return false;
        }
        //If partialHeap is empty
        if (this->numberOfItemsInPartialHeap == 0) {
            if ( this->_addItemsToPartialHeap() == false) {
                return false;
            }
        }
        if (this->numberOfItemsInPartialHeap != 0) {
            HeapItem *currentHeapMax = *(itemsHeap.begin());
            score = currentHeapMax->termRecordRuntimeScore;
            return true;
        } else {
            return false;
        }
    }
}

bool TermVirtualList::getNext(HeapItemForIndexSearcher *returnHeapItem)
{
    if (this->usingBitset) {
        returnHeapItem->recordId = this->bitSetIter->getNextRecordId();
        // When we use the bitset to get the next record for this virtual list, we don't have all the information for this record.
        returnHeapItem->termRecordRuntimeScore = 1.0;
        returnHeapItem->trieNode = 0;
        returnHeapItem->attributeIdsList.clear();
        returnHeapItem->ed = 0;
        returnHeapItem->positionIndexOffset = 0;
        if (returnHeapItem->recordId != RecordIdSetIterator::NO_MORE_RECORDS)
            return true;
        else
            return false;
    } else {
        // If partialHeap is empty
        if (this->numberOfItemsInPartialHeap == 0) {
            if (this->_addItemsToPartialHeap() == false)
                return false;
        }

        if (this->numberOfItemsInPartialHeap != 0) {
            // Elements are there in PartialHeap and pop them out to calling function
            HeapItem *currentHeapMax = *(itemsHeap.begin());
            pop_heap(itemsHeap.begin(), itemsHeap.begin() + this->numberOfItemsInPartialHeap,
                     TermVirtualList::HeapItemCmp());

            returnHeapItem->recordId = currentHeapMax->recordId;
            returnHeapItem->termRecordRuntimeScore = currentHeapMax->termRecordRuntimeScore;
            returnHeapItem->trieNode = currentHeapMax->trieNode;
            returnHeapItem->attributeIdsList = currentHeapMax->attributeIdsList;
            returnHeapItem->ed = currentHeapMax->ed;
            returnHeapItem->positionIndexOffset = currentHeapMax->positionIndexOffset;

            unsigned currentHeapMaxCursor = this->cursorVector[currentHeapMax->cursorVectorPosition];
            unsigned currentHeapMaxInvertetedListId = currentHeapMax->invertedListId;
            const shared_ptr<vectorview<unsigned> > &currentHeapMaxInvertedList = this->invertedListReadViewVector[currentHeapMax->cursorVectorPosition];
            unsigned currentHeapMaxInvertedListSize = currentHeapMaxInvertedList->size();

            bool foundValidHit = 0;

            // Check cursor is less than invertedList Size.
            while (currentHeapMaxCursor < currentHeapMaxInvertedListSize) {
                // InvertedList has more elements. Push invertedListElement at cursor into virtualList.

                unsigned recordId = currentHeapMaxInvertedList->getElement(currentHeapMaxCursor);
                // calculate record offset online
                unsigned keywordOffset = this->invertedIndex->getKeywordOffset(this->forwardIndexDirectoryReadView,
                		this->invertedIndexKeywordIdsReadView,
                		recordId, currentHeapMaxInvertetedListId);
                vector<unsigned> matchedAttributesList;
                currentHeapMaxCursor++;

                // check isValidTermPositionHit
                float termRecordStaticScore = 0;
                // ignore the record if it's no longer valid (e.g., marked deleted)
                if (keywordOffset != FORWARDLIST_NOTVALID &&
                        this->invertedIndex->isValidTermPositionHit(forwardIndexDirectoryReadView,
                        recordId,
                        keywordOffset,
                        term->getAttributesToFilter(), term->getFilterAttrOperation(), matchedAttributesList,
                        termRecordStaticScore)) {
                    foundValidHit = 1;
                    this->cursorVector[currentHeapMax->cursorVectorPosition] = currentHeapMaxCursor;
                    // Update cursor of popped virtualList in invertedListCursorVector.
                    // Cursor always points to next element in invertedList
                    currentHeapMax->recordId = recordId;
                    currentHeapMax->termRecordRuntimeScore =
                        DefaultTopKRanker::computeTermRecordRuntimeScore(termRecordStaticScore,
                                currentHeapMax->ed,
                                term->getKeyword()->size(),
                                currentHeapMax->isPrefixMatch,
                                this->prefixMatchPenalty , term->getSimilarityBoost());
                    currentHeapMax->attributeIdsList = matchedAttributesList;
                    currentHeapMax->positionIndexOffset = keywordOffset;
                    push_heap(itemsHeap.begin(), itemsHeap.begin()+this->numberOfItemsInPartialHeap,
                              TermVirtualList::HeapItemCmp());
                    break;
                }
            }

            if (!foundValidHit) {
                //InvertedList cursor end reached and so decrement number of elements in partialHeap
                this->numberOfItemsInPartialHeap--;
                //Delete the head of heap that represents empty converted list
                delete currentHeapMax;
                //TODO OPT Don't erase, accumulate and delete at the end.
                this->itemsHeap.erase(itemsHeap.begin()+this->numberOfItemsInPartialHeap);
            }

            return true;
        } else {
            return false;
        }
    }
}

void TermVirtualList::getPrefixActiveNodeSet(PrefixActiveNodeSet* &prefixActiveNodeSet)
{
    prefixActiveNodeSet = this->prefixActiveNodeSet;
}

void TermVirtualList::setCursors(std::vector<unsigned> *invertedListCursors)
{
    // TODO mar 1
    if (invertedListCursors != NULL) {
        unsigned a = invertedListCursors->size();
        unsigned b = this->cursorVector.size();
        (void)( a == b);

        //ASSERT(invertedListCursors->size() == this->cursorVector.size());
        copy(invertedListCursors->begin(), invertedListCursors->end(), this->cursorVector.begin());
    }
}

void TermVirtualList::getCursors(std::vector<unsigned>* &invertedListCursors)
{
    // TODO mar 1
    //invertedListCursors = new vector<unsigned>();
    if (invertedListCursors != NULL) {
        invertedListCursors->clear();
        invertedListCursors->resize(this->cursorVector.size());
        copy(this->cursorVector.begin(), this->cursorVector.end(), invertedListCursors->begin());
    }
}

void TermVirtualList::print_test() const
{
    Logger::debug("ItemsHeap Size %d", this->itemsHeap.size());
    for (vector<HeapItem* >::const_iterator heapIterator = this->itemsHeap.begin();
            heapIterator != this->itemsHeap.end(); heapIterator++) {
        Logger::debug("InvListPosition:\tRecord: %d\t Score:%.5f", (*heapIterator)->recordId, (*heapIterator)->termRecordRuntimeScore);
    }
}
unsigned TermVirtualList::getVirtualListTotalLength()
{
    if (this->usingBitset) {
        return bitSetSize;
    } else {
        unsigned totalLen = 0;
        for (unsigned i=0; i<itemsHeap.size(); i++) {
            totalLen += this->invertedListReadViewVector[itemsHeap[i]->cursorVectorPosition]->size();
        }
        return totalLen;
    }
}

}
}
