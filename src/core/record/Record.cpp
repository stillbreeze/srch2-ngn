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


#include <string>
#include <vector>

#include <instantsearch/Schema.h>
#include <instantsearch/Record.h>
#include "util/Assert.h"

#include "LocationRecordUtil.h"

#include <iostream> //for testing
#include <sstream>

using std::vector;
using std::string;

namespace srch2
{
namespace instantsearch
{

struct Record::Impl
{
    string primaryKey;
    std::vector<vector<string> > searchableAttributeValues;
    std::vector<string> refiningAttributeValues;
    float boost;
    const Schema *schema;
    std::string inMemoryRecordString;
    char * inMemoryStoredRecord;
    unsigned inMemoryStoredRecordLen;
    std::vector<string> roleIds;

    //Point : The location lat,lang value of the geo object
    Point point;
    ~Impl() {
    	if (inMemoryStoredRecord) {
    		delete[] inMemoryStoredRecord;
    	}
    }
};

Record::Record(const Schema *schema):impl(new Impl)
{
    impl->schema = schema;
    vector<string> emptyStringVector;
    // first we fill these two vectors with place holders.
    impl->searchableAttributeValues.assign(impl->schema->getNumberOfSearchableAttributes(), emptyStringVector);
    impl->refiningAttributeValues.assign(impl->schema->getNumberOfRefiningAttributes(),"");
    impl->boost = 1;
    impl->primaryKey = "";
    impl->point.x = 0;
    impl->point.y = 0;
    impl->inMemoryStoredRecord = 0;
    impl->inMemoryStoredRecordLen = 0;
}


Record::~Record()
{
    if (impl != NULL) {
        impl->schema = NULL;
        delete impl;
    }
}

bool Record::setSearchableAttributeValue(const string &attributeName,
        const string &attributeValue)
{
    int attributeId = impl->schema->getSearchableAttributeId(attributeName);
    if (attributeId < 0) {
        return false;
    }
    return setSearchableAttributeValue(attributeId, attributeValue);
}

bool Record::setSearchableAttributeValues(const string &attributeName,
		const std::vector<std::string> &attributeValues)
{
    int attributeId = impl->schema->getSearchableAttributeId(attributeName);
    if (attributeId < 0) {
        return false;
    }
    return setSearchableAttributeValues(attributeId, attributeValues);
}


bool Record::setSearchableAttributeValue(const unsigned attributeId,
                    const string &attributeValue)
{
    if (attributeId >= impl->schema->getNumberOfSearchableAttributes()) {
        return false;
    }

    // This function can only be called for a single-valued attribute
    ASSERT(impl->schema->isSearchableAttributeMultiValued(attributeId) == false);

    // For a single-valued attribute, we check searchableAttributeValues[attributeId].size().
    // If it's 0, do the assignment; otherwise, do an assert() and assign it to
    // the 0-th value.
    if (impl->searchableAttributeValues[attributeId].size() == 0) {
    	impl->searchableAttributeValues[attributeId].push_back(attributeValue);
    } else {
    	ASSERT(impl->searchableAttributeValues[attributeId].size() == 1);
    	impl->searchableAttributeValues[attributeId].at(0) = attributeValue;
    }

    return true;
}

bool Record::setSearchableAttributeValues(const unsigned attributeId,
		const std::vector<std::string> &attributeValues)
{
    if (attributeId >= impl->schema->getNumberOfSearchableAttributes()) {
        return false;
    }

    // This function can only be called for a multi-valued attribute
     ASSERT(impl->schema->isSearchableAttributeMultiValued(attributeId) == true);

    impl->searchableAttributeValues[attributeId] = attributeValues;
    return true;
}


bool Record::setRefiningAttributeValue(const std::string &attributeName,
            const std::string &attributeValue){
    int attributeId = impl->schema->getRefiningAttributeId(attributeName);
    if (attributeId < 0) {
        return false;
    }
    return setRefiningAttributeValue(attributeId, attributeValue);
}




bool Record::setRefiningAttributeValue(const unsigned attributeId,
                const std::string &attributeValue){
    if (attributeId >= impl->schema->getNumberOfRefiningAttributes()) {
        return false;
    }

    impl->refiningAttributeValues[attributeId] = attributeValue;
    return true;
}

void Record::setRoleIds(const std::vector<std::string> &roleId){
	impl->roleIds = roleId;
}

std::vector<std::string>* Record::getRoleIds() const{
	return &(this->impl->roleIds);
}

bool Record::hasRoleIds() const{
	if(this->impl->roleIds.size() == 0)
		return false;
	return true;
}


void Record::getSearchableAttributeValue(const unsigned attributeId, string & attributeValue) const
{
    if (attributeId >= impl->schema->getNumberOfSearchableAttributes())
    {
        return;
    }
    if(impl->searchableAttributeValues[attributeId].empty()){
    	return;
    }
    attributeValue = "";
    for(vector<string>::iterator attributeValueIter = impl->searchableAttributeValues[attributeId].begin() ;
    		attributeValueIter != impl->searchableAttributeValues[attributeId].end() ; ++attributeValueIter){
    	if(attributeValueIter == impl->searchableAttributeValues[attributeId].begin()){
    		attributeValue += MULTI_VAL_ATTR_DELIMITER;
    	}
    	attributeValue += *attributeValueIter;
    }
}

void Record::getSearchableAttributeValues(const unsigned attributeId , std::vector<string> & attributeStringValues) const {
    if (attributeId >= impl->schema->getNumberOfSearchableAttributes())
    {
        return;
    }
    attributeStringValues = impl->searchableAttributeValues[attributeId];
    return;
}

void Record::getSearchableAttributeValues(const string& attributeName ,
		std::vector<string> & attributeStringValues) const {

	int attributeId = impl->schema->getSearchableAttributeId(attributeName);
	if (attributeId < 0) {
		return;
	}
	getSearchableAttributeValues(attributeId, attributeStringValues);
}
std::string *Record::getRefiningAttributeValue(const unsigned attributeId) const
{
    if (attributeId >= impl->schema->getNumberOfRefiningAttributes())
    {
        return NULL;
    }
    return &impl->refiningAttributeValues[attributeId];
}

void Record::getRefiningAttributeValue(const string& attributeName, string& attributeValue) const
{
	int attributeId = impl->schema->getRefiningAttributeId(attributeName);
    if (attributeId < 0 || attributeId >= impl->schema->getNumberOfRefiningAttributes())
    {
        return;
    }
    attributeValue = impl->refiningAttributeValues[attributeId];
    return;
}

// add the primary key value
void Record::setPrimaryKey(const string &primaryKey)
{
    impl->primaryKey = primaryKey;
    int primaryKeyAttributeId = impl->schema->getSearchableAttributeId(*(impl->schema->getPrimaryKey()));

    ///if primaryKeyAttributeId is -1, there is no primaryKey in AttributeMap of schema,i.e primaryKey was not made searchable.
    if (primaryKeyAttributeId >= 0)
    {
        this->setSearchableAttributeValue(primaryKeyAttributeId, impl->primaryKey);
    }
}

// add the primary key value
void Record::setPrimaryKey(const unsigned &primaryKey)
{
    std::stringstream pkey_string;
    pkey_string << primaryKey;
    this->setPrimaryKey(pkey_string.str());
}

// get the primary key value
const string& Record::getPrimaryKey() const
{
    return impl->primaryKey;
}


// set/get the boost of this record (0 - 100)
float Record::getRecordBoost() const
{
    return impl->boost;
}

// required by Analyzer Tokenize
const Schema* Record::getSchema() const
{
    return impl->schema;
}
void Record::setInMemoryData(const void * ptr, unsigned len) {
	impl->inMemoryStoredRecord = new char[len];
	memcpy(impl->inMemoryStoredRecord, ptr, len);
	impl->inMemoryStoredRecordLen = len;
}

StoredRecordBuffer Record::getInMemoryData() const
{
	StoredRecordBuffer r = StoredRecordBuffer(impl->inMemoryStoredRecord, impl->inMemoryStoredRecordLen);
    return r;
}

void Record::setRecordBoost(const float recordBoost)
{
    impl->boost = recordBoost;
}

/**
 * Sets the location attribute value of the record.
 */
void Record::setLocationAttributeValue(const double &latitude, const double &longitude)
{
    impl->point.x = latitude;
    impl->point.y = longitude;
}

/**
 * Gets the location attribute value of the record.
 */
std::pair<double,double> Record::getLocationAttributeValue() const
{
    return std::make_pair<double,double>(impl->point.x, impl->point.y);
}

// clear the content of the record EXCEPT SCHEMA
void Record::clear()
{
    // We fill these two vectors with place holders to have the correct size.
    vector<string> emptyVector;
    impl->searchableAttributeValues.assign(impl->schema->getNumberOfSearchableAttributes(),emptyVector);
    impl->refiningAttributeValues.assign(impl->schema->getNumberOfRefiningAttributes(), "");
    impl->roleIds.clear();
    impl->boost = 1;
    impl->primaryKey = "";
    impl->inMemoryRecordString = "";
    impl->point.x = 0;
    impl->point.y = 0;
    if (impl->inMemoryStoredRecord) {
    	delete[] impl->inMemoryStoredRecord;
    	impl->inMemoryStoredRecord = NULL;
    }
}

void Record::disownInMemoryData() {
	// forget about it.
	impl->inMemoryStoredRecord = NULL;
}

}}

