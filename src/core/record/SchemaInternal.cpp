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

#include "SchemaInternal.h"
#include "util/Assert.h"
#include <map>
#include <vector>
#include <numeric>
#include <string>

using std::vector;
using std::string;
using std::map;
using std::pair;

namespace srch2 {
namespace instantsearch {

SchemaInternal::SchemaInternal(srch2::instantsearch::IndexType indexType,
        srch2::instantsearch::PositionIndexType positionIndexType) {
    this->indexType = indexType;
    this->positionIndexType = positionIndexType;
    this->scoringExpressionString = "1"; // DEFAULT SCORE
    this->refiningAttributeNameToId.clear();
    this->refiningAttributeDefaultValueVector.clear();
    this->refiningAttributeTypeVector.clear();
    this->refiningAttributeIsMultiValuedVector.clear();
    this->supportSwapInEditDistance = true;
    this->nameOfLatitudeAttribute = "";
    this->nameOfLongitudeAttribute = "";
}

SchemaInternal::SchemaInternal(const SchemaInternal &schemaInternal) {
    this->primaryKey = schemaInternal.primaryKey;
    this->searchableAttributeNameToId =
            schemaInternal.searchableAttributeNameToId;
    this->searchableAttributeTypeVector =
            schemaInternal.searchableAttributeTypeVector;
    this->searchableAttributeBoostVector =
            schemaInternal.searchableAttributeBoostVector;
    this->searchableAttributeIsMultiValuedVector =
            schemaInternal.searchableAttributeIsMultiValuedVector;
    this->refiningAttributeNameToId = schemaInternal.refiningAttributeNameToId;
    this->refiningAttributeTypeVector =
            schemaInternal.refiningAttributeTypeVector;
    this->refiningAttributeDefaultValueVector =
            schemaInternal.refiningAttributeDefaultValueVector;
    this->refiningAttributeIsMultiValuedVector =
            schemaInternal.refiningAttributeIsMultiValuedVector;
    this->searchableAttributeHighlightEnabled =
            schemaInternal.searchableAttributeHighlightEnabled;
    this->scoringExpressionString = schemaInternal.scoringExpressionString;
    this->indexType = schemaInternal.indexType;
    this->positionIndexType = schemaInternal.positionIndexType;
    this->commited = schemaInternal.commited;
    this->supportSwapInEditDistance = schemaInternal.supportSwapInEditDistance;

    this->aclRefiningAttrIds =  schemaInternal.aclRefiningAttrIds;
    this->nonAclRefiningAttrIds = schemaInternal.nonAclRefiningAttrIds;
    this->aclSearchableAttrIds =  schemaInternal.aclSearchableAttrIds;
    this->nonAclSearchableAttrIds = schemaInternal.nonAclSearchableAttrIds;

    this->nameOfLatitudeAttribute = schemaInternal.nameOfLatitudeAttribute;
    this->nameOfLongitudeAttribute = schemaInternal.nameOfLongitudeAttribute;
}

srch2::instantsearch::IndexType SchemaInternal::getIndexType() const {
    return this->indexType;
}

srch2::instantsearch::PositionIndexType SchemaInternal::getPositionIndexType() const {
    return this->positionIndexType;
}

unsigned SchemaInternal::getBoostSumOfSearchableAttributes() const {
    return std::accumulate(this->searchableAttributeBoostVector.begin(),
            this->searchableAttributeBoostVector.end(), 0);
}

int SchemaInternal::getSearchableAttributeId(
        const string &attributeName) const {
    map<string, unsigned>::const_iterator iter =
            this->searchableAttributeNameToId.find(attributeName.c_str());
    if (iter != this->searchableAttributeNameToId.end()) {
        return iter->second;
    } else {
        return -1;
    }
}

int SchemaInternal::getRefiningAttributeId(
        const std::string &refiningAttributeName) const {

    map<string, unsigned>::const_iterator iter =
            this->refiningAttributeNameToId.find(refiningAttributeName.c_str());
    if (iter != this->refiningAttributeNameToId.end()) {
        return iter->second;
    } else {
        return -1;
    }
}

SchemaInternal::~SchemaInternal() {

}

void SchemaInternal::setPrimaryKey(const string &primaryKey) {
    this->primaryKey = primaryKey;
}

void SchemaInternal::setNameOfLatitudeAttribute(const string &nameOfLatitudeAttribute){
	this->nameOfLatitudeAttribute = nameOfLatitudeAttribute;
}

void SchemaInternal::setNameOfLongitudeAttribute(const string &nameOfLongitudeAttribute){
	this->nameOfLongitudeAttribute = nameOfLongitudeAttribute;
}

const std::string* SchemaInternal::getPrimaryKey() const {
    return &this->primaryKey;
}

const std::string* SchemaInternal::getNameOfLatituteAttribute() const{
	return &this->nameOfLatitudeAttribute;
}

const std::string* SchemaInternal::getNameOfLongitudeAttribute() const{
	return &this->nameOfLongitudeAttribute;
}

int SchemaInternal::setSearchableAttribute(const string &attributeName,
        unsigned attributeBoost, bool isMultiValued, bool enableHiglight) {

    if (attributeBoost < 1 || attributeBoost > 100) {
        attributeBoost = 100;
    }

    // As of now (08/07/14), we assume each searchable attribute has a TEXT type.
    FilterType type = ATTRIBUTE_TYPE_TEXT;

    map<string, unsigned>::iterator iter;
    iter = this->searchableAttributeNameToId.find(attributeName);
    if (iter != this->searchableAttributeNameToId.end()) {
        this->searchableAttributeTypeVector[iter->second] = type;
        this->searchableAttributeBoostVector[iter->second] = attributeBoost;
        this->searchableAttributeIsMultiValuedVector[iter->second] =
                isMultiValued;
        this->searchableAttributeHighlightEnabled[iter->second] =
                enableHiglight;
        return iter->second;
    } else {
        int searchAttributeMapSize = this->searchableAttributeNameToId.size();
        this->searchableAttributeNameToId[attributeName] =
                searchAttributeMapSize;
        this->searchableAttributeTypeVector.push_back(type);
        this->searchableAttributeBoostVector.push_back(attributeBoost);
        this->searchableAttributeIsMultiValuedVector.push_back(isMultiValued);
        this->searchableAttributeHighlightEnabled.push_back(enableHiglight);
    }
    return this->searchableAttributeNameToId.size() - 1;
}

int SchemaInternal::setRefiningAttribute(const std::string &attributeName,
        FilterType type, const std::string & defaultValue, bool isMultiValued) {

    map<string, unsigned>::iterator iter;
    iter = this->refiningAttributeNameToId.find(attributeName);
    if (iter != this->refiningAttributeNameToId.end()) {
        this->refiningAttributeDefaultValueVector[iter->second] = defaultValue;
        this->refiningAttributeTypeVector[iter->second] = type;
        this->refiningAttributeIsMultiValuedVector[iter->second] =
                isMultiValued;
        return iter->second;
    } else {
        unsigned sizeNonSearchable = this->refiningAttributeNameToId.size();
        this->refiningAttributeNameToId[attributeName] = sizeNonSearchable;

        this->refiningAttributeDefaultValueVector.push_back(defaultValue);
        this->refiningAttributeTypeVector.push_back(type);
        this->refiningAttributeIsMultiValuedVector.push_back(isMultiValued);
    }
    return this->refiningAttributeNameToId.size() - 1;

}

const std::string* SchemaInternal::getDefaultValueOfRefiningAttribute(
        const unsigned nonSearchableAttributeNameId) const {
    if (nonSearchableAttributeNameId
            < this->refiningAttributeDefaultValueVector.size()) {
        return &this->refiningAttributeDefaultValueVector[nonSearchableAttributeNameId];
    } else {
        return NULL;
    }
    return NULL;
}

void SchemaInternal::setSupportSwapInEditDistance(
        const bool supportSwapInEditDistance) {
    this->supportSwapInEditDistance = supportSwapInEditDistance;
}

bool SchemaInternal::getSupportSwapInEditDistance() const {
    return supportSwapInEditDistance;
}

void SchemaInternal::setScoringExpression(
        const std::string &scoringExpression) {
    this->scoringExpressionString = scoringExpression;
}

const std::string SchemaInternal::getScoringExpression() const {
    return this->scoringExpressionString;
}

FilterType SchemaInternal::getTypeOfRefiningAttribute(
        const unsigned refiningAttributeNameId) const {

    if (refiningAttributeNameId >= this->refiningAttributeTypeVector.size()) {
        ASSERT(false);
        return srch2::instantsearch::ATTRIBUTE_TYPE_TEXT;
    }
    return this->refiningAttributeTypeVector[refiningAttributeNameId];

}

FilterType SchemaInternal::getTypeOfSearchableAttribute(
        const unsigned searchableAttributeNameId) const {

    if (searchableAttributeNameId
            >= this->searchableAttributeTypeVector.size()) {
        ASSERT(false);
        return srch2::instantsearch::ATTRIBUTE_TYPE_TEXT;
    }
    return this->searchableAttributeTypeVector[searchableAttributeNameId];
}

const std::map<std::string, unsigned> * SchemaInternal::getRefiningAttributes() const {
    return &this->refiningAttributeNameToId;
}

bool SchemaInternal::isRefiningAttributeMultiValued(
        const unsigned refiningAttributeNameId) const {
    if (refiningAttributeNameId
            >= this->refiningAttributeIsMultiValuedVector.size()) {
        ASSERT(false);
        return false;
    }
    return this->refiningAttributeIsMultiValuedVector[refiningAttributeNameId];
}

unsigned SchemaInternal::getBoostOfSearchableAttribute(
        const unsigned attributeId) const {
    if (attributeId < this->searchableAttributeBoostVector.size()) {
        return this->searchableAttributeBoostVector[attributeId];
    } else {
        return 0;
    }
}

/*
 * Returns true if this searchable attribute is multivalued
 */
bool SchemaInternal::isSearchableAttributeMultiValued(
        const unsigned searchableAttributeNameId) const {
    if (searchableAttributeNameId
            < this->searchableAttributeBoostVector.size()) {
        return this->searchableAttributeIsMultiValuedVector[searchableAttributeNameId];
    } else {
        ASSERT(false);
        return false;
    }
}

const std::map<std::string, unsigned>& SchemaInternal::getSearchableAttribute() const {
    return this->searchableAttributeNameToId;
}

unsigned SchemaInternal::getNumberOfSearchableAttributes() const {
    return this->searchableAttributeNameToId.size();
}

unsigned SchemaInternal::getNumberOfRefiningAttributes() const {

    return this->refiningAttributeNameToId.size();
}

bool SchemaInternal::isHighlightEnabled(unsigned attributeId) const {
    if (attributeId < this->searchableAttributeHighlightEnabled.size()) {
        return this->searchableAttributeHighlightEnabled[attributeId];
    } else {
        return false;
    }
}

void SchemaInternal::setPositionIndexType(PositionIndexType positionIndexType) {
	this->positionIndexType = positionIndexType;
}

void SchemaInternal::setAclSearchableAttrIdsList(const std::vector<unsigned>& aclSearchableAttrIds){
	this->aclSearchableAttrIds = aclSearchableAttrIds;
}
void SchemaInternal::setNonAclSearchableAttrIdsList(const std::vector<unsigned>& nonAclSearchableAttrIds){
	this->nonAclSearchableAttrIds = nonAclSearchableAttrIds;
}

void SchemaInternal::setAclRefiningAttrIdsList(const std::vector<unsigned>& aclRefiningAttrIds){
	this->aclRefiningAttrIds = aclRefiningAttrIds;
}
void SchemaInternal::setNonAclRefiningAttrIdsList(const std::vector<unsigned>& nonAclRefiningAttrIds){
	this->nonAclRefiningAttrIds = nonAclRefiningAttrIds;
}

const std::vector<unsigned>& SchemaInternal::getAclSearchableAttrIdsList() const{
	return this->aclSearchableAttrIds;
}
const std::vector<unsigned>& SchemaInternal::getNonAclSearchableAttrIdsList() const{
	return this->nonAclSearchableAttrIds;
}
const std::vector<unsigned>& SchemaInternal::getAclRefiningAttrIdsList() const{
	return this->aclRefiningAttrIds;
}
const std::vector<unsigned>& SchemaInternal::getNonAclRefiningAttrIdsList() const{
	return this->nonAclRefiningAttrIds;
}

bool SchemaInternal::isValidAttribute(const std::string& attributeName) const {
	int id = this->getSearchableAttributeId(attributeName);
	if (id != -1) {
		return true;
	} else {
		id = this->getRefiningAttributeId(attributeName);
		if (id != -1) {
			return true;
		} else {
			return false;
		}
	}
}

}
}
