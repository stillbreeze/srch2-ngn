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
#ifndef __FORWARDINDEX_H__
#define __FORWARDINDEX_H__

#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/map.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/unordered_set.hpp>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>

#include <instantsearch/Record.h>
#include "record/SchemaInternal.h"
#include "analyzer/AnalyzerInternal.h"
#include "util/cowvector/cowvector.h"
#include "util/Logger.h"
#include "util/encoding.h"
#include "util/half.h"
#include "instantsearch/TypedValue.h"
#include "util/mytime.h"
#include "util/ULEB128.h"
#include "thirdparty/snappy-1.0.4/snappy.h"
#include "util/ThreadSafeMap.h"
using std::vector;
using std::fstream;
using std::string;
using std::map;
using std::pair;
using half_float::half;
using namespace snappy;

// The upper bound of the number of keywords in a record is FFFFFF
#define KEYWORD_THRESHOLD ((1<<24) - 1)
// The upper bound of the number of sortable attributes in a record is FF
#define SORTABLE_ATTRIBUTES_THRESHOLD ((1<<8) - 1)

// an indicator that a forward list has been physically deleted
#define FORWARDLIST_NOTVALID (unsigned (-1))

namespace srch2 {
namespace instantsearch {

typedef vector<pair<unsigned, pair<string, unsigned> > > KeywordIdKeywordStringInvertedListIdTriple;

// for reordering keyword ids
struct KeywordRichInformation {
    unsigned keywordId;

    /*
     * The product of term frequency (TF) and summation of attribute boosts.
     * This product doesn't change in the life cycle of this record.
     */
    float keywordTfBoostProduct;

    float keywordScore;
    vector<uint8_t> keywordAttribute;
    vector<uint8_t> keywordPositionsInAllAttribute;
    vector<uint8_t> keywordOffsetsInAllAttribute;
    vector<uint8_t> keywordSynonymBitMapInAllAttribute;
    vector<uint8_t> keywordSynonymCharLenInAllAttribute;
    bool operator <(const KeywordRichInformation& keyword) const {
        return keywordId < keyword.keywordId;
    }
};

typedef pair<unsigned, pair<unsigned, unsigned> > NewKeywordIdKeywordOffsetTriple;
struct NewKeywordIdKeywordOffsetPairGreaterThan {
    bool operator()(const NewKeywordIdKeywordOffsetTriple &lhs,
            const NewKeywordIdKeywordOffsetTriple &rhs) const {
        return lhs.first > rhs.first;
    }
};

/*
 *   this class keeps the id of roles that have access to a record
 *   we keep an object of this class in forward list for each record
 */
class RecordAcl{
private:
	vector<string> roles;
	mutable boost::shared_mutex mutexRW;

public:
	RecordAcl(){};
	~RecordAcl(){};

	// this function will return false if this roleId already exists
	bool appendRole(string &roleId){
		vector<string>::iterator it;
		boost::unique_lock<boost::shared_mutex> lock(mutexRW);
		bool roleExisted = findRole(roleId, it);
		if(!roleExisted)
			this->roles.insert(it, roleId);
		lock.unlock();
		return !roleExisted;
	}

	// return false if this role id doesn't exit
	bool deleteRole(const string &roleId){
		vector<string>::iterator it;
		boost::unique_lock<boost::shared_mutex> lock(mutexRW);
		bool roleExisted = findRole(roleId, it);
		if(roleExisted)
			this->roles.erase(it);
		lock.unlock();
		return roleExisted;
	}

	// check whether a role id exits in the access list or not
	bool hasRole(string &roleId){
		vector<string>::iterator it;
		boost::shared_lock< boost::shared_mutex> lock(mutexRW);
		bool roleExisted = findRole(roleId, it);
		lock.unlock();
		return roleExisted;
	}

	void clearRoles(){
		boost::unique_lock<boost::shared_mutex> lock(mutexRW);
		roles.clear();
	}

	/*
	 *  we use this function for deleting a record.
	 *  and becuase we only have one writer at a moment we don't need to use the lock.
	 */
	vector<string>& getRoles(){
		return this->roles;
	}

	void print(){
		std::cout << "----roles----" << std::endl;
		for(unsigned i = 0 ; i < roles.size(); ++i){
			std::cout << roles[i] << std::endl;
		}
		std::cout << "------------" << std::endl;
	}
private:
	bool findRole(const string &roleId, std::vector<string>::iterator &it){
		it = std::lower_bound(this->roles.begin(), this->roles.end(), roleId);
		if(it == this->roles.end())
			return false;
		if( *it != roleId )
			return false;
		return true;
	}

	friend class boost::serialization::access;

	template<class Archive>
	void serialize(Archive & ar, const unsigned int version) {
		ar & this->roles;
	}

};

// get the count of set bits in the number
unsigned getBitSet(unsigned number);
// get the count of bit set before the bit position of attributeId
unsigned getBitSetPositionOfAttr(unsigned bitmap, unsigned attribute);
class ForwardList {
public:

    //getter and setter
    unsigned getNumberOfKeywords() const {
        return numberOfKeywords;
    }

    void setNumberOfKeywords(unsigned numberOfKeywords) {
        this->numberOfKeywords = numberOfKeywords;
    }

    float getRecordBoost() const {
        return recordBoost;
    }

    void setRecordBoost(float recordBoost) {
        this->recordBoost = recordBoost;
    }

    std::string getExternalRecordId() const {
        return externalRecordId;
    }

    void setExternalRecordId(std::string externalRecordId) {
        this->externalRecordId = externalRecordId;
    }

    StoredRecordBuffer getInMemoryData() const {
    	StoredRecordBuffer r = StoredRecordBuffer(this->inMemoryData, this->inMemoryDataLen);
    	return r;
    }

    void setInMemoryData(const StoredRecordBuffer& inMemoryData) {
        this->inMemoryData = inMemoryData.start;
        this->inMemoryDataLen = inMemoryData.length;
    }

//    const std::string getRefiningAttributeValue(unsigned iter,
//            const Schema * schema) const {
//        return VariableLengthAttributeContainer::getAttribute(iter, schema, this->getRefiningAttributeValuesDataPointer());
//    }

    /*
     * The format of data in this array is :
     * ---------------------------------------------------------------------------------------------------------------------------------
     * | keywordIDs | keywordRecordTfBoostProduct | keywordRecordStaticScores | nonSearchableAttributeValues | keywordAttributeBitMap | positionIndex |  offsetIndex |
     * ---------------------------------------------------------------------------------------------------------------------------------
     *
     * This function will calculate and prepare nonSearchableAttribute byte array in its place.
     * and will allocate the whole space and copy the last part (positional index)
     * the rest of data will be filled out through setKeywordId(...) , setKeywordRecordStaticScore(...)
     * and setKeywordAttributeBitmap(...) API calls.
     */
    void allocateSpaceAndSetNSAValuesAndPosIndex(const Schema * schema,
    		vector<uint8_t>& AttributeIdsVector,
    		vector<uint8_t>& positionIndexDataVector,
    		vector<uint8_t>& offsetIndexDataVector,
    		vector<uint8_t>& charLenDataVector,
    		vector<uint8_t>& synonymBitMapVector){

    	this->attributeIdsIndexSize = AttributeIdsVector.size();
    	this->positionIndexSize = positionIndexDataVector.size();
    	this->offsetIndexSize = offsetIndexDataVector.size();
    	this->charLenIndexSize = charLenDataVector.size();
    	this->synonymBitMapSize = synonymBitMapVector.size();

    	 // first three blocks are for keywordIDs, keywordTBoostProducts and keywordRecordStaticScores.
    	dataSize = getKeywordIdsSizeInBytes() + getKeywordTfBoostProductsSizeInBytes()
    	        + getKeywordRecordStaticScoresSizeInBytes();
    	data = new Byte[dataSize +
    	                this->getKeywordAttributeIdsSize() +
    	                this->getPositionIndexSize() + this->offsetIndexSize
    	                + this->synonymBitMapSize +  this->charLenIndexSize];

    	// third block is attributeBitmap
    	/////
    	copy(AttributeIdsVector.begin() , AttributeIdsVector.end(), data + this->dataSize);
    	dataSize = dataSize + this->getKeywordAttributeIdsSize();

    	// last part is positionIndex
    	/////
    	copy(positionIndexDataVector.begin() , positionIndexDataVector.end(), data + this->dataSize);
    	dataSize = dataSize + this->getPositionIndexSize();

    	copy(offsetIndexDataVector.begin() , offsetIndexDataVector.end(), data + this->dataSize);
    	dataSize = dataSize + this->offsetIndexSize;

    	copy(synonymBitMapVector.begin() , synonymBitMapVector.end(), data + this->dataSize);
    	dataSize = dataSize + this->synonymBitMapSize;

    	copy(charLenDataVector.begin() , charLenDataVector.end(), data + this->dataSize);
    	dataSize = dataSize + this->charLenIndexSize;

    }

    void copyByteArraysToForwardList(vector<uint8_t>& AttributeIdsVector,
    		vector<uint8_t>& positionIndexDataVector,
    		vector<uint8_t>& offsetIndexDataVector,
    		vector<uint8_t>& synonymBitMapVector,
    		vector<uint8_t>& charLenDataVector){

    	ASSERT(this->attributeIdsIndexSize == AttributeIdsVector.size());
    	ASSERT(this->positionIndexSize == positionIndexDataVector.size());
    	ASSERT(this->offsetIndexSize == offsetIndexDataVector.size());
    	ASSERT(this->synonymBitMapSize == synonymBitMapVector.size());
    	ASSERT(this->charLenIndexSize == charLenDataVector.size());

    	unsigned dataSize = getKeywordIdsSizeInBytes() + getKeywordRecordStaticScoresSizeInBytes();
    	// copy attribute ids
    	copy(AttributeIdsVector.begin() , AttributeIdsVector.end(), data + dataSize);
    	dataSize = dataSize + this->getKeywordAttributeIdsSize();


    	// copy position index
    	copy(positionIndexDataVector.begin() , positionIndexDataVector.end(), data + dataSize);
    	dataSize = dataSize + this->getPositionIndexSize();

    	// copy char offset index
    	copy(offsetIndexDataVector.begin() , offsetIndexDataVector.end(), data + dataSize);
    	dataSize = dataSize + this->offsetIndexSize;

    	// copy synonym bit map
    	copy(synonymBitMapVector.begin() , synonymBitMapVector.end(), data + dataSize);
    	dataSize = dataSize + this->synonymBitMapSize;

    	// copy synonym's original char length
    	copy(charLenDataVector.begin() , charLenDataVector.end(), data + dataSize);
    	dataSize = dataSize + this->charLenIndexSize;

    	ASSERT(dataSize == this->dataSize);

    }

    const unsigned* getKeywordIds() const {
        return getKeywordIdsPointer();
    }

    const unsigned getKeywordId(unsigned iter) const {
        return getKeywordIdsPointer()[iter];
    }

    void setKeywordId(unsigned iter, unsigned keywordId) {
        if (iter <= KEYWORD_THRESHOLD)
            this->getKeywordIdsPointer()[iter] = keywordId;
    }

    //TF * sumOfFieldBoosts
    const float getKeywordTfBoostProduct(unsigned iter) const {
        return getKeywordTfBoostProductsPointer()[iter];
    }

    //TF * sumOfFieldBoosts
    void setKeywordTfBoostProduct(unsigned iter, float keywordTfBoostProduct) {
        if (iter <= KEYWORD_THRESHOLD)
            this->getKeywordTfBoostProductsPointer()[iter] = keywordTfBoostProduct;
    }

    const float getKeywordRecordStaticScore(unsigned iter) const {
        return getKeywordRecordStaticScoresPointer()[iter];
    }

    void setKeywordRecordStaticScore(unsigned iter, float keywordScore) {
        if (iter <= KEYWORD_THRESHOLD)
            this->getKeywordRecordStaticScoresPointer()[iter] = keywordScore;
    }

    uint8_t * getKeywordAttributesListPtr() const{
    	return getKeywordAttributeIdsPointer();
    }

    void getKeywordAttributeIdsList(unsigned keywordOffset, vector<unsigned>& attributeIdList) const;
    void getKeywordAttributeIdsLists(const unsigned numOfKeywords,  //Get all the attribute id lists in one loop
            vector<vector<unsigned> > & attributeIdsLists) const;

    void getKeywordAttributeIdsByteArray(unsigned keywordOffset, vector<uint8_t>& attributesVLBarray);
    void getKeyWordPostionsByteArray(unsigned keywordOffset, vector<uint8_t>& positionsVLBarray);
    void getKeyWordOffsetsByteArray(unsigned keywordOffset, vector<uint8_t>& charOffsetsVLBarray);
    void getKeywordSynonymsBitMapByteArray(unsigned keywordOffset, vector<uint8_t>& synonymBitMapVLBarray);
    void getSynonymCharLensByteArray(unsigned keywordOffset, vector<uint8_t>& synonymCharLensVLBarray);
    void fetchVLBArrayForKeyword(unsigned keyOffset, const uint8_t * piPtr, vector<uint8_t>& vlbArray);

    //set the size of keywordIds and keywordRecordStaticScores to keywordListCapacity
    ForwardList(int keywordListCapacity = 0) {

        // we consider at most KEYWORD_THRESHOLD keywords
        if (keywordListCapacity > KEYWORD_THRESHOLD)
            keywordListCapacity = KEYWORD_THRESHOLD;
        numberOfKeywords = 0;
        recordBoost = 0.5;
        inMemoryData.reset();
        inMemoryDataLen = 0;
        // the dataSize and data are initialized temporarily. They will actually be initialized in
        // allocateSpaceAndSetNSAValuesAndPosIndex when other pieces of data are also ready.
        dataSize = 0;
        data = NULL;
        attributeIdsIndexSize = 0;
        positionIndexSize = 0;
        offsetIndexSize = 0;
        charLenIndexSize = 0;
        synonymBitMapSize = 0;
    }

    virtual ~ForwardList() {
        if(data != NULL){
        	delete[] data;  // data is allocated as an array with new[]
        }
    }

    float computeFieldBoostSummation(const Schema *schema,
            const TokenAttributeHits &hits) const;

    //unsigned getForwardListElement(unsigned cursor) const;

    bool haveWordInRangeWithStemmer(const SchemaInternal* schema,
            const unsigned minId, const unsigned maxId,
            const unsigned termSearchableAttributeIdToFilterTermHits,
            unsigned &matchingKeywordId,
            unsigned &matchingKeywordAttributeBitmap,
            float &matchingKeywordRecordStaticScore, bool &isStemmed) const;
    bool haveWordInRange(const SchemaInternal* schema, const unsigned minId,
            const unsigned maxId,
            const vector<unsigned>& filteringAttributesList, ATTRIBUTES_OP atrOps,
            unsigned &keywordId, vector<unsigned>& matchingKeywordAttributesList,
            float &termRecordStaticScore) const;

    unsigned getKeywordOffset(unsigned keywordId) const;
    unsigned getKeywordOffsetByLinearScan(unsigned keywordId) const;
    void computeTermFrequencies(const unsigned numOfKeywords, vector<float> & keywordTfList) const;

    bool getWordsInRange(const SchemaInternal* schema, const unsigned minId,
            const unsigned maxId,
            const vector<unsigned>& filteringAttributesList,
            ATTRIBUTES_OP attrOps,
            vector<unsigned> &keywordIdsVector) const;

    /**************************PositionIndex****************/
    float getTermRecordStaticScore(unsigned forwardIndexId,
            unsigned keywordOffset) const;
    /**
     * Input @param tokenAttributeHitsMap is a map of keywordTokens to a "vector of positions" for the given record.
     * The PositionIndex is ZERO terminated, i.e. ZERO is the end flag for each position list.
     * So, keyword positions in each record attribute start from 1.
     * For example,
     * Consider a record, a1 a2 a1 a1 a3 a1 a3
     * keywordIdMap has the following key,value pairs:
     * a1: [1, 3, 4, 7]
     * a2: [2]
     * a3: [6, 8]
     * Here, a1,a2,a3 are keywordTokesn.
     *
     * Internally, addRecord iterates over the keys and for each key, the "positions" are first appended and then a [0] is appended to the positionVector.
     * In the above case, for key a1, we append [1, 3, 4, 7, 0].
     * for key a2, we append [2, 0].
     * for key a3, we append [6, 8, 0].
     */

    bool isValidRecordTermHit(const SchemaInternal *schema,
            unsigned keywordOffset,const vector<unsigned>& filteringAttributesList, ATTRIBUTES_OP attrOp,
            vector<unsigned>& matchingKeywordAttributesList, float& termRecordStaticScore) const;
    bool isValidRecordTermHitWithStemmer(const SchemaInternal *schema,
            unsigned keywordOffset, unsigned searchableAttributeId,
            unsigned &matchingKeywordAttributeBitmap,
            float &termRecordStaticScore, bool &isStemmed) const;

    unsigned getNumberOfBytes() const;


    //void mapOldIdsToNewIds();

    // Position Indexes APIs
    void getKeyWordPostionsInRecordField(unsigned keywordOffset, unsigned attributeId,
    		vector<unsigned>& positionList) const;
    void getKeywordTfListInRecordField(vector<float> & keywordTfList) const;
    void getKeywordTfListFromVLBArray(const uint8_t * piPtr,
            vector<float> & keywordTfList) const;
    void fetchDataFromVLBArray(unsigned keyOffset, unsigned attributeId,
    		vector<unsigned>& pl, const uint8_t * piPtr) const;
    void getKeyWordOffsetInRecordField(unsigned keyOffset, unsigned attributeId,
    		vector<unsigned>& pl) const;
    void getSynonymCharLenInRecordField(unsigned keyOffset, unsigned attributeId,
    		vector<unsigned>& pl) const;
    void getSynonymBitMapInRecordField(unsigned keyOffset, unsigned attributeId,
    		vector<uint8_t>& synonymBitMap) const;

    bool accessibleByRole(string &roleId){
    	return this->recordAcl.hasRole(roleId);
    };

    void appendRolesToResource(vector<string> &roleIds){
    	for (unsigned i = 0 ; i < roleIds.size() ; i++){
    		this->recordAcl.appendRole(roleIds[i]);
    	}
    }

    void deleteRolesFromResource(vector<string> &roleIds){
    	for (unsigned i = 0 ; i < roleIds.size() ; i++){
    		this->recordAcl.deleteRole(roleIds[i]);
    	}
    }

    void deleteRoleFromResource(const string &roleId){
    	this->recordAcl.deleteRole(roleId);
    }

    RecordAcl* getAccessList(){
    	return &(this->recordAcl);
    }

private:
    friend class boost::serialization::access;

    template<class Archive>
    void serialize(Archive & ar, const unsigned int version) {
        typename Archive::is_loading load;
        ar & this->numberOfKeywords;
        ar & this->recordBoost;
        ar & this->attributeIdsIndexSize;
        ar & this->positionIndexSize;
        ar & this->offsetIndexSize;
        ar & this->synonymBitMapSize;
        ar & this->charLenIndexSize;
        ar & this->dataSize;
        ar & this->recordAcl;
        if (this->inMemoryData.get() == NULL)
        	this->inMemoryDataLen = 0;
        ar & this->inMemoryDataLen;
        /*
         * Since we don't have access to ForwardIndex and we don't know whether attributeBasedSearch is on, our encodin
         * scheme is :
         * first save/load the size of keywordAttributeBitmaps array, and then if size was not zero also save/load the array itself.
         *
         */
        // In loading process, we need to allocate space for the members first.
        if (load) {
        	this->data = new Byte[this->dataSize];
        	if (inMemoryDataLen > 0)
        		this->inMemoryData.reset(new char[inMemoryDataLen]);
        	else
        		this->inMemoryData.reset();
        }
        ar
                & boost::serialization::make_array(this->data,
                        this->dataSize);
        ar & this->externalRecordId;
        if (this->inMemoryDataLen > 0)
        	ar & boost::serialization::make_array((char *)this->inMemoryData.get(), this->inMemoryDataLen);
    }

    // members
    unsigned numberOfKeywords;
    half recordBoost;
    std::string externalRecordId;
    boost::shared_ptr<const char> inMemoryData;
    unsigned inMemoryDataLen;
    RecordAcl recordAcl;


    /*
     * The format of data in this array is :
     * ------------------------------------------------------------------------------------------------------------------------
     * | keywordIDs | keywordTfBoostProducts | keywordRecordStaticScores | keywordAttributeBitMap | positionIndex | offsetIndex |
     * ------------------------------------------------------------------------------------------------------------------------
     */
    Byte * data;

    unsigned attributeIdsIndexSize;
    unsigned positionIndexSize;
    unsigned dataSize;
    unsigned offsetIndexSize;
    unsigned charLenIndexSize;
    unsigned synonymBitMapSize;


    ///////////////////     Keyword IDs Helper Functions //////////////////////////////////////
    inline unsigned * getKeywordIdsPointer() const {
        /*
         * The format of data in this array is :
         * ------------------------------------------------------------------------------------------------------------------------
         * | keywordIDs | keywordTfBoostProducts | keywordRecordStaticScores | keywordAttributeBitMap | positionIndex | offsetIndex |
         * ------------------------------------------------------------------------------------------------------------------------
         */
    	// keyword IDs start from the 0th position.
    	return (unsigned *)(data + 0);
    }
    inline unsigned getKeywordIdsSizeInBytes(unsigned sizeInUnsigned) const {

        return sizeInUnsigned * (sizeof(unsigned) / sizeof(Byte));
    }
    inline unsigned getKeywordIdsSizeInBytes() const {
        return getKeywordIdsSizeInBytes(this->getNumberOfKeywords());
    }

    ////////////////////  Keyword TF Boost Scores Helper Fucntions /////////////////////////
    /*
     * Our ranking formula is : text_relevance = tf * idf * sumOfFieldBoosts
     * Since the tf and sumOfFieldBoosts do not change during the lifecycle of a record,
     * it's not necessary to repeatedly compute them during the merge and commit phase.
     */
    inline half* getKeywordTfBoostProductsPointer() const {
        /*
         * The format of data in this array is :
         * ------------------------------------------------------------------------------------------------------------------------
         * | keywordIDs | keywordTfBoostProducts | keywordRecordStaticScores | keywordAttributeBitMap | positionIndex | offsetIndex |
         * ------------------------------------------------------------------------------------------------------------------------
         */
        return (half *)(data + getKeywordIdsSizeInBytes());
    }
    inline unsigned getKeywordTfBoostProductsSizeInBytes(unsigned sizeInHalf) const {
        return sizeInHalf * (sizeof(half) / sizeof(Byte));
    }
    inline unsigned getKeywordTfBoostProductsSizeInBytes() const {
        return getKeywordTfBoostProductsSizeInBytes(this->getNumberOfKeywords());
    }


    ////////////////////  Keyword Record Static Scores Helper Fucntions /////////////////////////
    inline half* getKeywordRecordStaticScoresPointer() const {
        /*
         * The format of data in this array is :
         * ------------------------------------------------------------------------------------------------------------------------
         * | keywordIDs | keywordTfBoostProducts | keywordRecordStaticScores | keywordAttributeBitMap | positionIndex | offsetIndex |
         * ------------------------------------------------------------------------------------------------------------------------
         */
        return (half *)(data + getKeywordIdsSizeInBytes() + getKeywordTfBoostProductsSizeInBytes());
    }
    inline unsigned getKeywordRecordStaticScoresSizeInBytes(unsigned sizeInHalf) const {
        return sizeInHalf * (sizeof(half) / sizeof(Byte));
    }
    inline unsigned getKeywordRecordStaticScoresSizeInBytes() const {
        return getKeywordRecordStaticScoresSizeInBytes(this->getNumberOfKeywords());
    }


    //////////////////// Keyword Attributes Bitmap Helper Functions ////////////////////////////
    inline uint8_t * getKeywordAttributeIdsPointer() const {
        /*
         * The format of data in this array is :
         * ------------------------------------------------------------------------------------------------------------------------
         * | keywordIDs | keywordTfBoostProducts | keywordRecordStaticScores | keywordAttributeBitMap | positionIndex | offsetIndex |
         * ------------------------------------------------------------------------------------------------------------------------
         */
        return (uint8_t *) (data + getKeywordIdsSizeInBytes()
                + getKeywordTfBoostProductsSizeInBytes()
                + getKeywordRecordStaticScoresSizeInBytes());
    }

    inline unsigned getKeywordAttributeIdsSize() const {
        return attributeIdsIndexSize;
    }

    /////////////////////// Position Index Helper Functions //////////////////////////////
    inline uint8_t * getPositionIndexPointer() const {
        /*
         * The format of data in this array is :
         * ------------------------------------------------------------------------------------------------------------------------
         * | keywordIDs | keywordTfBoostProducts | keywordRecordStaticScores | keywordAttributeBitMap | positionIndex | offsetIndex |
         * ------------------------------------------------------------------------------------------------------------------------
         */
        return (uint8_t *) (data + getKeywordIdsSizeInBytes()
                + getKeywordTfBoostProductsSizeInBytes()
                + getKeywordRecordStaticScoresSizeInBytes()
                + getKeywordAttributeIdsSize());
    }
    inline uint8_t * getOffsetIndexPointer() const {
        /*
         * The format of data in this array is :
         * ------------------------------------------------------------------------------------------------------------------------
         * | keywordIDs | keywordTfBoostProducts | keywordRecordStaticScores | keywordAttributeBitMap | positionIndex | offsetIndex |
         * ------------------------------------------------------------------------------------------------------------------------
         */
        return getPositionIndexPointer() + positionIndexSize;
    }
    inline uint8_t * getSynonymBitMapPointer() const {
        /*
         * The format of data in this array is :
         * ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
         * | keywordIDs | keywordTfBoostProducts | keywordRecordStaticScores | keywordAttributeBitMap | positionIndex |  charOffsetIndex | SynonymBitFlagArray | SynonymOriginalTokenLenArray |
         * ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
         */
        return getOffsetIndexPointer() + offsetIndexSize;
    }
    inline uint8_t * getCharLenIndexPointer() const {
        /*
         * The format of data in this array is :
         * ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
         * | keywordIDs | keywordTfBoostProducts | keywordRecordStaticScores | keywordAttributeBitMap | positionIndex |  charOffsetIndex | SynonymBitFlagArray | SynonymOriginalTokenLenArray |
         * ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
         */
        return getSynonymBitMapPointer() + synonymBitMapSize;
    }
    inline unsigned getPositionIndexSize() {
        return positionIndexSize;
    }
    inline unsigned getOffsetIndexSize() {
        return offsetIndexSize;
    }

};

typedef std::pair<ForwardList*, bool> ForwardListPtr;

/**
 * A forward index includes a forward list for each record.  The list (sorted)
 * includes the keywords in the record, represented as keywordIds, representing the trie leaf node
 * corresponding to the keyword.
 */
class ForwardIndex {
private:
    const static unsigned MAX_SCORE = unsigned(-1);

    ///vector of forwardLists, where RecordId is the element index.
    cowvector<ForwardListPtr> *forwardListDirectory;
    ReadViewManager<ForwardListPtr> fwdListDirReadViewsMgr;
    //Used only in WriteView
    ThreadSafeMap<std::string, unsigned> externalToInternalRecordIdMap;

    // Build phase structure
    // Stores the order of records, by which it was added to forward index. Used in bulk initial insert
    //std::vector<unsigned> recordOrder;

    // Set to true when the commit function is called. Set to false, when the addRecord function is called.
    bool commited_WriteView;
    bool mergeRequired;
    boost::unordered_set<unsigned> deletedRecordInternalIds; // store internal IDs of records marked deleted

    // Initialised in constructor and used in calculation of offset in filterAttributesVector. This is lighter than serialising the schema itself.
    const SchemaInternal *schemaInternal;

    friend class boost::serialization::access;

    template<class Archive>
    void serialize(Archive & ar, const unsigned int version) {
        ar & *forwardListDirectory;
        ar & externalToInternalRecordIdMap;
        ar & commited_WriteView;
    }

public:

    // is attribute based search or not
    bool isAttributeBasedSearch;

    ForwardIndex(const SchemaInternal* schemaInternal);
    ForwardIndex(const SchemaInternal* schemaInternal,
            unsigned expectedNumberOfDocumentsToInitialize);
    virtual ~ForwardIndex();

    unsigned getTotalNumberOfForwardLists_ReadView() const;
    unsigned getTotalNumberOfForwardLists_WriteView() const;

    void addDummyFirstRecord(); // For Trie bootstrap

    /*
     * Returns the forward list directory read view
     */
    void getForwardListDirectory_ReadView(shared_ptr<vectorview<ForwardListPtr> > & readView) const;

    /**
     * Add a new record, represented by a list of keyword IDs. The keywordIds are unique and have no order.
     * If the record ID already exists, do nothing. Else, add the record.
     */
    void addRecord(const Record *record, const unsigned recordId,
            KeywordIdKeywordStringInvertedListIdTriple &keywordIdList,
            map<string, TokenAttributeHits> &tokenAttributeHitsMap);

    bool appendRoleToResource(shared_ptr<vectorview<ForwardListPtr> > & forwardListDirectoryReadView, const string& resourcePrimaryKeyID, vector<string> &roleIds);

    bool deleteRoleFromResource(shared_ptr<vectorview<ForwardListPtr> > & forwardListDirectoryReadView, const string& resourcePrimaryKeyID, vector<string> &roleIds);

    bool deleteRoleFromResource(shared_ptr<vectorview<ForwardListPtr> > & forwardListDirectoryReadView, const string& resourcePrimaryKeyID, const string &roleId);

    RecordAcl* getRecordAccessList(shared_ptr<vectorview<ForwardListPtr> > & forwardListDirectoryReadView, const string& resourcePrimaryKeyID);

    /**
     * Set the deletedFlag on the forwardList, representing record deletion.
     */
    void setDeleteFlag(unsigned internalRecordId);

    /**
     * Resume the deletedFlag on the forwardList, reversing record deletion.
     */
    void resetDeleteFlag(unsigned internalRecordId);

    /**
     * Verify if given interval is present in the forward list
     */
    //bool havePrefixInForwardList(const unsigned recordId, const unsigned minId, const unsigned maxId, const int termSearchableAttributeIdToFilterTermHits, float &score) const;
    /**
     * Check if the ForwardList of recordId has a keywordId in range [minId, maxId]. Note that this is closed range.
     * If the recordId does not exist, return false.
     */
    ///Added for stemmer
    bool haveWordInRangeWithStemmer(shared_ptr<vectorview<ForwardListPtr> > & forwardListDirectoryReadView,
    		const unsigned recordId,
            const unsigned minId, const unsigned maxId,
            const unsigned termSearchableAttributeIdToFilterTermHits,
            unsigned &matchingKeywordId,
            unsigned &matchingKeywordAttributeBitmap,
            float &matchingKeywordRecordStaticScore, bool &isStemmed) const;
    bool haveWordInRange(shared_ptr<vectorview<ForwardListPtr> > & forwardListDirectoryReadView ,
    		const unsigned recordId,
    		const unsigned minId,
            const unsigned maxId,
            const vector<unsigned>& filteringAttributesList,
            ATTRIBUTES_OP attrOp,
            unsigned &matchingKeywordId,
            vector<unsigned>& matchingKeywordAttributesList,
            float &matchingKeywordRecordStaticScore) const;

    /**
     * Check if the ForwardList of recordId has a keywordId in range [minId, maxId]. Note that this is closed range.
     * If the recordId does not exist, return false. - STEMMER VERSION
     */
    //bool haveWordInRangeWithStemmer(const unsigned recordId, const unsigned minId, const unsigned maxId, const int termSearchableAttributeIdToFilterTermHits, unsigned &keywordId, float &termRecordStaticScore, bool& isStemmed);
    /**
     * Returns the number of bytes occupied by the ForwardIndex
     */
    unsigned getNumberOfBytes() const;

    static void exportData(ForwardIndex &forwardIndex, const string &exportedDataFileName);

    /**
     * Build Phase functions
     */
    const ForwardList *getForwardList(shared_ptr<vectorview<ForwardListPtr> > & forwardListDirectoryReadView,
    		unsigned recordId, bool &valid) const;

    bool hasAccessToForwardList(shared_ptr<vectorview<ForwardListPtr> > & forwardListDirectoryReadView,
    		unsigned recordId, string &roleId);

    //ForwardList *getForwardListToChange(unsigned recordId, bool &valid);
    ForwardList *getForwardList_ForCommit(unsigned recordId);

    //ForwardListElement *getForwardListElementByOffset(unsigned forwardIndexOffset);

    /** COMMITING FORWARDINDEX
     * 1. Get the invertedIndex.
     * 2. Set all hitCounts in ForwardIndexDirectory to zero.
     * 3. For each invertedListDirectory element -> keywordId, get newKeywordId, i.e
     *      1. For each RecordId in Inverted List
     *          1. Insert into forwardIndex using the following steps.
     *              1. Look up ForwardIndexDirectory and get offset, hitcount.
     *              2. Insert at offset+hitcount, the newKeywordId
     *              3. Increment hitcount
     * 4. Sort each ForwardList based on newKeywordIds
     */
    void commit();
    void merge();
    //void commit(ForwardList *forwardList, const vector<unsigned> *oldIdToNewIdMap, vector<NewKeywordIdKeywordOffsetTriple> &forwardListReOrderAtCommit );
    void commit(ForwardList *forwardList,
            const map<unsigned, unsigned> &oldIdToNewIdMapper,
            vector<NewKeywordIdKeywordOffsetTriple> &forwardListReOrderAtCommit);

    void reorderForwardList(ForwardList *forwardList,
            const map<unsigned, unsigned> &oldIdToNewIdMapper,
            vector<NewKeywordIdKeywordOffsetTriple> &forwardListReOrderAtCommit);
    void finalCommit();
    //void commit(const std::vector<unsigned> *oldIdToNewIdMap);

    bool isCommitted() const {
        return this->commited_WriteView;
    }
    bool isMergeRequired() const { return mergeRequired; }

    // Free the space of those forward lists that have been marked deleted
    // The caller must acquire the necessary lock to make sure
    // no readers can access the forward index since some of
    // its forward lists are being freed.
    void freeSpaceOfDeletedRecords();
    bool hasDeletedRecords() { return deletedRecordInternalIds.size() > 0; }
    void setSchema(SchemaInternal *schema) {
        this->schemaInternal = schema;
    }

    const SchemaInternal *getSchema() const {
        return this->schemaInternal;
    }

    //void print_test() const;
    void print_test();
    void print_size() const;

    float getTermRecordStaticScore(unsigned forwardIndexId,
            unsigned keywordOffset) const;
    /**
     * Input @param tokenAttributeHitsMap is a map of keywordTokens to a "vector of positions" for the given record.
     * The PositionIndex is ZERO terminated, i.e. ZERO is the end flag for each position list.
     * So, keyword positions in each record attribute start from 1.
     * For example,
     * Consider a record, a1 a2 a1 a1 a3 a1 a3
     * keywordIdMap has the following key,value pairs:
     * a1: [1, 3, 4, 7]
     * a2: [2]
     * a3: [6, 8]
     * Here, a1,a2,a3 are keywordTokesn.
     *
     * Internally, addRecord iterates over the keys and for each key, the "positions" are first appended and then a [0] is appended to the positionVector.
     * In the above case, for key a1, we append [1, 3, 4, 7, 0].
     * for key a2, we append [2, 0].
     * for key a3, we append [6, 8, 0].
     */
    bool isValidRecordTermHit(shared_ptr<vectorview<ForwardListPtr> > & forwardListDirectoryReadView,
    		unsigned forwardIndexId,
    		unsigned keywordOffset,
    		const vector<unsigned>& filterAttributesList, ATTRIBUTES_OP attrOp,
    		vector<unsigned> &matchingKeywordAttributesList,
            float& termRecordStaticScore) const;
    bool isValidRecordTermHitWithStemmer(unsigned forwardIndexId,
            unsigned keywordOffset, unsigned searchableAttributeId,
            unsigned &matchingKeywordAttributeBitmap,
            float &matchingKeywordRecordStaticScore, bool &isStemmed) const;

    // return FORWARDLIST_NOTVALID if the forward is not valid (e.g., already deleted)
    unsigned getKeywordOffset(shared_ptr<vectorview<ForwardListPtr> > & forwardListDirectoryReadView,
    		unsigned forwardListId, unsigned keywordId) const;

    /*****-record-id-converter*****/
    /**
     * Append @param[in] externalRecordId to the RecordIdVector
     * @param[out] internalRecordId is the internal recordID corresponding to externalRecordId.
     * Internally, internalRecordId is the uint value of the position where the externalRecordId was appended, i.e, "RecordIdVector.size() - 1".
     *
     * If the externalRecordId already exists, "false" is returned. internalRecordId is set to a default (unsigned)(-1) in this case.
     */
    //bool appendExternalRecordId(unsigned externalRecordId, unsigned &internalRecordId);// Goes to both map-write and forwardIndex
    void appendExternalRecordIdToIdMap(const std::string &externalRecordId,
            unsigned &internalRecordId);

    bool deleteRecord(const std::string &externalRecordId);
    bool deleteRecordGetInternalId(
            const std::string &externalRecordId, unsigned &internalRecordId);
    //bool deleteExternalRecordId(unsigned externalRecordId); // Goes to forwardIndex and map-write

    bool recoverRecord(const std::string &externalRecordId,
            unsigned internalRecordId);

    INDEXLOOKUP_RETVAL lookupRecord(
            const std::string &externalRecordId, unsigned& internalRecordId) const;

    void reassignKeywordIds(shared_ptr<vectorview<ForwardListPtr> > & forwardListDirectoryReadView,
    		const unsigned recordId,
            const map<unsigned, unsigned> &keywordIdMapper);
    //void reassignKeywordIds(map<unsigned, unsigned> &reassignedKeywordIdMapper);

    /**
     * For the given internalRecordId, returns the ExternalRecordId. ASSERT if the internalRecordId
     * is out of bound of RecordIdVector.
     */
    bool getExternalRecordIdFromInternalRecordId(
    		shared_ptr<vectorview<ForwardListPtr> > & forwardListDirectoryReadView,
    		const unsigned internalRecordId,
            std::string &externalRecordId) const; // Goes to forwardIndex-read

    bool getInternalRecordIdFromExternalRecordId(const std::string &externalRecordId,
            unsigned &internalRecordId) const; // Goes to recordIdConverterMap-read

    /**
     * Access the InMemoryData of a record using InternalRecordId
     */
    StoredRecordBuffer getInMemoryData(unsigned internalRecordId) const;

    void convertToVarLengthArray(const vector<unsigned>& positionListVector,
    							 vector<uint8_t>& grandBuffer);
    void convertToVarLengthBitMap(const vector<uint8_t>& bitMapVector,
    		vector<uint8_t>& grandBuffer);
};


void fetchCommonAttributes(const vector<unsigned>& list1, const vector<unsigned>& list2,
		vector<unsigned>& outList);
bool isAttributesListsMatching(const vector<unsigned>& list1, const vector<unsigned>& list2);

}
}

#endif //__FORWARDINDEX_H__
