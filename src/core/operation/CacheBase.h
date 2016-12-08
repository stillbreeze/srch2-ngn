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

#ifndef __CACHEBASE_H__
#define __CACHEBASE_H__

#include <instantsearch/GlobalCache.h>
#include <instantsearch/Term.h>
#include "util/BusyBit.h"
#include "operation/ActiveNode.h"
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/locks.hpp>
#include <string>
#include <map>

using namespace std;

namespace srch2
{
namespace instantsearch
{

template <class T>
class CacheEntry{
public:
	CacheEntry(string & key, boost::shared_ptr<T> & objectPointer){
		this->setKey(key);
		this->setObjectPointer(objectPointer);
		this->numberOfBytes = 0;
	}

	void setKey(string & key){
		this->key = key;
	}
	string getKey(){
		return key;
	}

	void setObjectPointer(boost::shared_ptr<T> & objectPointer){
		this->objectPointer = objectPointer;
	}
	boost::shared_ptr<T> getObjectPointer(){
		return this->objectPointer;
	}

	unsigned getNumberOfBytes(){
		if(numberOfBytes == 0){
			numberOfBytes = sizeof(numberOfBytes) + sizeof(this->key) + this->objectPointer->getNumberOfBytes();
		}
		return numberOfBytes;
	}
private:
	string key;
	boost::shared_ptr<T> objectPointer;
	unsigned numberOfBytes ;
};

/*
 * this class is the main implementation of cache which contains the cache entries and
 * get/set behaviors.
 */
template <class T>
class CacheContainer{

	struct HashedKeyLinkListElement{
		HashedKeyLinkListElement * next;
		HashedKeyLinkListElement * previous;
		unsigned hashedKey;
		HashedKeyLinkListElement(unsigned hashedKey){
			this->hashedKey = hashedKey;
			this->next = NULL;
			this->previous = NULL;
		}
	};

public:
	CacheContainer(unsigned long byteSizeOfCache = 134217728)
	: cacheTotalByteBudget(byteSizeOfCache){
		totalSizeUsed = 0;
		elementsLinkListFirst = elementsLinkListLast = NULL;
	}
	~CacheContainer(){
		boost::unique_lock< boost::shared_mutex > lock(_access);
		HashedKeyLinkListElement * nextToDelete = elementsLinkListFirst;
		while(nextToDelete != NULL){
			HashedKeyLinkListElement * nextOfNextToDelete = nextToDelete->next;
			delete nextToDelete;
			nextToDelete = nextOfNextToDelete;
		}
		for(typename map<unsigned , pair< CacheEntry<T> * , HashedKeyLinkListElement * > >::iterator cacheEntry = cacheEntries.begin();
				cacheEntry != cacheEntries.end() ; ++cacheEntry){
			delete cacheEntry->second.first ;
		}
		cacheEntries.clear();
		lock.unlock();
	};

	bool put(string & key, boost::shared_ptr<T> & objectPointer){

		boost::unique_lock< boost::shared_mutex > lock(_access);

//		ASSERT(checkCacheConsistency());
		CacheEntry<T> * newEntry = new CacheEntry<T>(key , objectPointer);
		unsigned numberOfBytesNeededForNewEntry = getNumberOfBytesUsedByEntry(newEntry);
		if(numberOfBytesNeededForNewEntry > cacheTotalByteBudget){
			// we cannot accept this entry, it's bigger than our budget
			// Cache is isolated from other modules. No one can "assume" something is in cache and must
			// always call get() to be able to use it. So not inserting it in cache and returning false is
			// safe.
			delete newEntry;
			lock.unlock();
//			ASSERT(checkCacheConsistency());
			return false;
		}
		if(numberOfBytesNeededForNewEntry > cacheTotalByteBudget - totalSizeUsed){ // size is bigger than our left budget, we should remove some entries
			// keep removing entries until enough space is left
			while(true){
				LRUCacheReplacementPolicyKickoutOneEntry();
				if(numberOfBytesNeededForNewEntry <= cacheTotalByteBudget - totalSizeUsed){
					break;
				}
			}
		}
		// now we can insert this new entry
		// 1. first find the hashedKey
		unsigned newEntryHashedKey = hashDJB2(key.c_str());
		// 2. check to see if this hashKey is in the map
		typename map<unsigned , pair< CacheEntry<T> * , HashedKeyLinkListElement * > >::iterator cacheEntryWithSameHashedKey = cacheEntries.find(newEntryHashedKey);
		if(cacheEntryWithSameHashedKey != cacheEntries.end()){ // hashed key exists
			// replace the old cache entry with the new one and update the size info
			unsigned sizeOfEntryToRemove = getNumberOfBytesUsedByEntry(cacheEntryWithSameHashedKey->second.first);
			// update the size
			// it's important that we += first and then -= because it's unsigned
			totalSizeUsed += numberOfBytesNeededForNewEntry;
			ASSERT(totalSizeUsed >= sizeOfEntryToRemove);
			totalSizeUsed -= sizeOfEntryToRemove;
			ASSERT(totalSizeUsed <= cacheTotalByteBudget);
			// This delete operation deletes the instance of CacheEntry. Since
			// ts_shared_ptr<T> objectPointer is a member, it's destructor will be called
			// and the counter of shared pointer is decremented by one.
			delete cacheEntryWithSameHashedKey->second.first;
			cacheEntryWithSameHashedKey->second.first = newEntry;
			// move the linked list element to front to make it kicked out later
			moveLinkedListElementToLast(cacheEntryWithSameHashedKey->second.second);
			lock.unlock();
//			ASSERT(checkCacheConsistency());
			return true;
		}
		// 3. if it's not in map push it to the map, append hashedKey to the linkedlist and update the size
		addNewElementToLinkList(newEntryHashedKey );
		cacheEntries[newEntryHashedKey] = std::make_pair(  newEntry, elementsLinkListLast );
		totalSizeUsed += numberOfBytesNeededForNewEntry;
		ASSERT(totalSizeUsed <= cacheTotalByteBudget);
		lock.unlock();
//		ASSERT(checkCacheConsistency());
		return true;
	}
	bool get(string & key, boost::shared_ptr<T> & objectPointer) {

		boost::unique_lock< boost::shared_mutex > lock(_access);
//		ASSERT(checkCacheConsistency());
		//1. compute the hashed key
		unsigned hashedKeyToFind = hashDJB2(key.c_str());
		typename map<unsigned , pair< CacheEntry<T> * , HashedKeyLinkListElement * > >::iterator cacheEntry = cacheEntries.find(hashedKeyToFind);
		if(cacheEntry == cacheEntries.end()){ // hashed key doesn't exist
			lock.unlock();
//			ASSERT(checkCacheConsistency());
			return false;
		}
		if(cacheEntry->second.first->getKey().compare(key) != 0){
			lock.unlock();
//			ASSERT(checkCacheConsistency());
			return false;
		}
		// cache hit , move the corresponding linked list element to the last position to make it
		// get kicked out laster
		moveLinkedListElementToLast(cacheEntry->second.second);
		// and return the object
		objectPointer = cacheEntry->second.first->getObjectPointer();
		lock.unlock();
//		ASSERT(checkCacheConsistency());
		return true;
	}


	bool checkCacheConsistency() {
		boost::shared_lock< boost::shared_mutex > lock(_access);
		// 1 . check the consistency of linked list
		if( elementsLinkListFirst == NULL && elementsLinkListLast == NULL ){
			lock.unlock();
			return true;
		}else{
			if(! (elementsLinkListFirst != NULL && elementsLinkListLast != NULL)){
				lock.unlock();
				return false;
			}
		}
		// we know first and last are not null

		if(elementsLinkListFirst->previous != NULL){
			lock.unlock();
			return false;
		}
		HashedKeyLinkListElement * nextToCheck = elementsLinkListFirst;
		while(nextToCheck != NULL){
			if(nextToCheck->previous == NULL){
				if(nextToCheck != elementsLinkListFirst){
					lock.unlock();
					return false;
				}
			}else{
				if(nextToCheck->previous->next != nextToCheck){
					lock.unlock();
					return false;
				}
			}
			if(nextToCheck->next == NULL){
				if(nextToCheck != elementsLinkListLast){
					lock.unlock();
					return false;
				}
			}else{
				if(nextToCheck->next->previous != nextToCheck){
					lock.unlock();
					return false;
				}
			}
			HashedKeyLinkListElement * nextOfNextToCheck = nextToCheck->next;
			nextToCheck = nextOfNextToCheck;
		}
		// 2. check the consistency of map and byte size
		unsigned byteSize = 0;
		for(typename map<unsigned , pair< CacheEntry<T> * , HashedKeyLinkListElement * > >::const_iterator cacheEntry = cacheEntries.begin();
				cacheEntry != cacheEntries.end() ; ++cacheEntry){
			bool nodeFound = false;
			HashedKeyLinkListElement * nextToCheck = elementsLinkListFirst;
			while(nextToCheck != NULL){
				if(cacheEntry->first == nextToCheck->hashedKey){
					if(nextToCheck != cacheEntry->second.second){
						lock.unlock();
						return false;
					}
					if(nodeFound){
						lock.unlock();
						return false;
					}else{
						nodeFound = true;
					}
				}
				HashedKeyLinkListElement * nextOfNextToCheck = nextToCheck->next;
				nextToCheck = nextOfNextToCheck;
			}
			if(nodeFound == false){
				lock.unlock();
				return false;
			}
			byteSize += getNumberOfBytesUsedByEntry(cacheEntry->second.first);
		}
		if(byteSize != totalSizeUsed){
			lock.unlock();
			return false;
		}
		lock.unlock();
		return true;

	}

	bool clear(){
		boost::unique_lock<boost::shared_mutex> lock(_access);

		for(typename map<unsigned , pair< CacheEntry<T> * , HashedKeyLinkListElement * > >::iterator cacheEntry = cacheEntries.begin();
				cacheEntry != cacheEntries.end() ; ++cacheEntry){
			delete cacheEntry->second.first ;
		}
		cacheEntries.clear();
		HashedKeyLinkListElement * nextToDelete = elementsLinkListFirst;
		while(nextToDelete != NULL){
			HashedKeyLinkListElement * nextOfNextToDelete = nextToDelete->next;
			delete nextToDelete;
			nextToDelete = nextOfNextToDelete;
		}
		elementsLinkListFirst = elementsLinkListLast = NULL;
		totalSizeUsed = 0;
		lock.unlock();
		return true;
	}
private:
	mutable boost::shared_mutex _access;

	map<unsigned , pair< CacheEntry<T> * , HashedKeyLinkListElement * > > cacheEntries;

	// LRU replacement policy always removes elements from the beginning
	HashedKeyLinkListElement * elementsLinkListFirst;
	// LRU replacement policy always requires adding new entries to the tail (after last)
	HashedKeyLinkListElement * elementsLinkListLast;

	const unsigned long cacheTotalByteBudget;
	unsigned totalSizeUsed;


	void LRUCacheReplacementPolicyKickoutOneEntry(){
		// 1. get the oldest element to kick out, which is the first element on linked list
		unsigned hashedKeyToRemove ;
		if(removeTheOldestElementFromLinkList(hashedKeyToRemove) == false){
			return; // cache is empty , these is nothing to remove
		}

		// 2. remove the entry from the map and update the total byte size used
		typename map<unsigned , pair< CacheEntry<T> * , HashedKeyLinkListElement * > >::iterator entryToRemove = cacheEntries.find(hashedKeyToRemove);
		ASSERT(entryToRemove != cacheEntries.end());
		unsigned numberOfUsedBytesToGetRidOf = getNumberOfBytesUsedByEntry(entryToRemove->second.first);
		// This delete operation deletes the instance of CacheEntry. Since
		// ts_shared_ptr<T> objectPointer is a member, it's destructor will be called
		// and the counter of shared pointer is decremented by one.
		delete entryToRemove->second.first;
		cacheEntries.erase(entryToRemove); // remove it from map
		ASSERT(totalSizeUsed >= numberOfUsedBytesToGetRidOf);
		totalSizeUsed -= numberOfUsedBytesToGetRidOf;
	}

	// does not update the size
	void addNewElementToLinkList(unsigned hashedKey){
		ASSERT((elementsLinkListFirst == NULL && elementsLinkListLast == NULL) ||
				(elementsLinkListFirst != NULL && elementsLinkListLast != NULL) );

		if(elementsLinkListFirst == NULL && elementsLinkListLast == NULL){
			elementsLinkListFirst = elementsLinkListLast = new HashedKeyLinkListElement(hashedKey);
			return;
		}

		ASSERT(elementsLinkListLast->next == NULL);

		elementsLinkListLast->next = new HashedKeyLinkListElement(hashedKey);
		elementsLinkListLast->next->previous = elementsLinkListLast;
		elementsLinkListLast = elementsLinkListLast->next;
	}

	// does not update the size , just removes the first element of linked list
	bool removeTheOldestElementFromLinkList(unsigned & removedHashedKey){
		ASSERT((elementsLinkListFirst == NULL && elementsLinkListLast == NULL) ||
				(elementsLinkListFirst != NULL && elementsLinkListLast != NULL) );
		if(elementsLinkListFirst == NULL && elementsLinkListLast == NULL){
			return false; // nothing in linked list to remove
		}
		if(elementsLinkListFirst == elementsLinkListLast){ // there is only one element
			removedHashedKey = elementsLinkListFirst->hashedKey;
			delete elementsLinkListFirst;
			elementsLinkListFirst = elementsLinkListLast = NULL;
			return true;
		}
		HashedKeyLinkListElement * newFirst = elementsLinkListFirst->next;
		ASSERT(newFirst->previous == elementsLinkListFirst);
		newFirst->previous = NULL;
		removedHashedKey = elementsLinkListFirst->hashedKey;
		delete elementsLinkListFirst;
		elementsLinkListFirst = newFirst;
		return true;
	}

	void moveLinkedListElementToLast(HashedKeyLinkListElement * element){
		ASSERT(element != NULL);
		ASSERT(! (elementsLinkListFirst == NULL || elementsLinkListLast == NULL));
		if(elementsLinkListFirst == elementsLinkListLast){
			// only one entry, no need to move anything
			return;
		}
		if(element == elementsLinkListLast){ // the element is already on the last position
			ASSERT(element->next == NULL);
			// no need to do anything
			return;
		}
		if(element == elementsLinkListFirst){ // the element is the first element
			ASSERT(element->previous == NULL);
			ASSERT(element->next->previous == element);
			elementsLinkListFirst = element->next;
			elementsLinkListFirst->previous = NULL;
			ASSERT(elementsLinkListLast->next == NULL);
			elementsLinkListLast->next = element;
			element->previous = elementsLinkListLast;
			element->next = NULL;
			elementsLinkListLast = element;
			return;
		}
		// element is in the middle
		// 1. connect previous to next and next to previous
		element->previous->next = element->next;
		element->next->previous = element->previous;
		element->next = element->previous = NULL;
		// 2. put element in the front and make it HEAD
		ASSERT(elementsLinkListLast->next == NULL);
		elementsLinkListLast->next = element;
		element->previous = elementsLinkListLast;
		elementsLinkListLast = element;
		return;
	}

	unsigned getNumberOfBytesUsedByEntry(CacheEntry<T> * entry){
		/*
		 * entry + unsigned hashedKey + linkedList element
		 */
		return entry->getNumberOfBytes() + sizeof(unsigned) + sizeof(HashedKeyLinkListElement *) +  sizeof(HashedKeyLinkListElement);
	}

	// computes the hash value of a string
	unsigned hashDJB2(const char *str) const
	{
	    unsigned hash = 5381;
	    unsigned c;
	    do
	    {
	        c = *str;
	        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	    }while(*str++);
	    return hash;
	}
};



}
}

#endif // __CACHEBASE_H__
