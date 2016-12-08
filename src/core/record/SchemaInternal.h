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

#ifndef __SCHEMAINTERNAL_H__
#define __SCHEMAINTERNAL_H__

#include <instantsearch/platform.h>
#include <instantsearch/Schema.h>

#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/map.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

#include <fstream>
#include <map>
#include <vector>
#include <numeric>
#include <string>

using std::fstream;

namespace srch2 {
namespace instantsearch {

/**
 * This class defines a schema of records, which describes the
 * structure of records put into an index. It consists of a
 * "PrimaryKey" and a set of [AttributeId, AttributeName, AttributeBoost] triples.
 * The PrimaryKey is an string that uniquely identifies a record.
 * The PrimaryKey can also be made searchable by adding it as one
 * of the attributes.
 *
 * A schema can have at most 255 attributes, including the primary
 * key.
 *
 *  - AttributeId: The Id of an attribute in the list of attribute triples.
 *  It depends on the order in which the atrribute were added to the list. The index start
 *  at 0 for the first added attribute, than 1 for the second added attribute and so on.
 *  Note that AttributeId is set internally and the API doesnot allow setting the AttributeId.
 *
 *  - AttributeName: Name of the attribute
 *
 *  - AttributeBoost: Boost value as the importance of this field. A
 * larger AttributeBoost value represents a higher importance of
 * the attribute. A boost value is in the range [1,100].
 *
 *
 * For example, consider a collection of customer records with the
 * following schema:
 *
 *  - PhoneNumber: the PrimaryKey.
 *
 *  - [0, CustName, 50], [1, Address, 10], [2, emailID, 20] are a set of
 *    [AttributeId, AttributeName, AttributeBoost] triples. Here, CustName has a
 *    boost value of 50.
 *
 *  - If we want to search for records using PhoneNumber, we can
 *    make the PrimaryKey searchable by adding it as one of the
 *    [AttributeId, AttributeName, AttributeBoost] triple, say [3, PhoneNumber, 60].
 */
class SchemaInternal: public Schema {
public:

    /**
     * Creates a Schema object
     */
    SchemaInternal() {
    }
    ;
    SchemaInternal(srch2::instantsearch::IndexType indexType,
            srch2::instantsearch::PositionIndexType positionIndexType);
    SchemaInternal(const SchemaInternal &schemaInternal);

    srch2::instantsearch::IndexType getIndexType() const;

    srch2::instantsearch::PositionIndexType getPositionIndexType() const;

    /**
     *  Sets the name of the primary key to primaryKey.
     */
    void setPrimaryKey(const std::string &primaryKey);

    // sets the name of the latitude attribute
    void setNameOfLatitudeAttribute(const std::string &nameOfLatitudeAttribute);

    // sets the name of the longitude attribute
    void setNameOfLongitudeAttribute(const std::string &nameOfLatitudeAttribute);

    /**
     * \ingroup RankingFunctions Sets the boost value of an attribute.
     *  @param attributeName  Name of the attribute.
     *  @param attributeBoost The boost value in the range [1-100].
     */
    int setSearchableAttribute(const std::string &attributeName,
            unsigned attributeBoost = 1, bool isMultiValued = false,
            bool higlightEnabled = false);

    int setRefiningAttribute(const std::string &attributeName, FilterType type,
            const std::string & defaultValue, bool isMultiValued = false);

    /**
     * Returns the AttributeName of the primaryKey
     */
    const std::string* getPrimaryKey() const;

    // Returns the AttributeName of the latitude
    const std::string* getNameOfLatituteAttribute() const;

    //Returns the AttributeName of the longitude
    const std::string* getNameOfLongitudeAttribute() const;

    /**
     * Gets the boost value of the attribute with
     * attributeId.  \return the boost value of the attribute;
     * "0" if the attribute does not exist.
     */
    unsigned getBoostOfSearchableAttribute(
            const unsigned searchableAttributeNameId) const;
    /*
     * Returns true if this searchable attribute is multivalued
     */
    bool isSearchableAttributeMultiValued(
            const unsigned searchableAttributeNameId) const;

    const std::map<std::string, unsigned>& getSearchableAttribute() const;
    /**
     * Gets the sum of all attribute boosts in the schema.  The
     * returned value can be used for the normalization of
     * attribute boosts.
     */
    unsigned getBoostSumOfSearchableAttributes() const;
    /**
     * @returns the number of attributes in the schema.
     */
    unsigned getNumberOfSearchableAttributes() const;
    /**
     * Gets the index of an attribute name by doing an internal
     * lookup. The index of an attribute depends on the order in
     * which the function setAtrribute was called. The index start
     * at 0 for the first added attribute, than 1 and so on.
     * @return the index if the attribute is found, or -1 otherwise.
     */
    int getSearchableAttributeId(
            const std::string &searchableAttributeName) const;

    const std::string* getDefaultValueOfRefiningAttribute(
            const unsigned searchableAttributeNameId) const;
    FilterType getTypeOfRefiningAttribute(
            const unsigned refiningAttributeNameId) const;
    FilterType getTypeOfSearchableAttribute(
            const unsigned searchableAttributeNameId) const;
    int getRefiningAttributeId(
            const std::string &searchableAttributeName) const;
    unsigned getNumberOfRefiningAttributes() const;
    const std::map<std::string, unsigned> * getRefiningAttributes() const;
    bool isRefiningAttributeMultiValued(
            const unsigned nonSearchableAttributeNameId) const;

    int commit() {
        this->commited = 1;
        return 0;
    }

    void setSupportSwapInEditDistance(const bool supportSwapInEditDistance);
    bool getSupportSwapInEditDistance() const;

    void setScoringExpression(const std::string &scoringExpression);
    const std::string getScoringExpression() const;

    virtual bool isHighlightEnabled(unsigned id) const;

    void setPositionIndexType(PositionIndexType positionIndexType);

    virtual void setAclSearchableAttrIdsList(const std::vector<unsigned>& aclEnabledFieldIds);
    virtual void setNonAclSearchableAttrIdsList(const std::vector<unsigned>& nonAclEnabledFieldIds);
    virtual void setAclRefiningAttrIdsList(const std::vector<unsigned>& aclEnabledFieldIds);
    virtual void setNonAclRefiningAttrIdsList(const std::vector<unsigned>& nonAclEnabledFieldIds);

    virtual const std::vector<unsigned>& getAclSearchableAttrIdsList() const;
    virtual const std::vector<unsigned>& getNonAclSearchableAttrIdsList() const;
    virtual const std::vector<unsigned>& getAclRefiningAttrIdsList() const;
    virtual const std::vector<unsigned>& getNonAclRefiningAttrIdsList() const;

    bool isValidAttribute(const std::string& attributeName) const;

    /**
     * Destructor to free persistent resources used by the Schema
     */
    virtual ~SchemaInternal();

private:
    std::string primaryKey;

    /**
     * TODO:change to bi - directional map
     * http://www.boost.org/doc/libs/1_37_0/libs/multi_index/doc/index.html
     * http://stackoverflow.com/questions/535317/checking-value-exist-in-a-stdmap-c
     */
    std::map<std::string, unsigned> searchableAttributeNameToId;
    std::vector<FilterType> searchableAttributeTypeVector;

    std::vector<unsigned> searchableAttributeBoostVector;
    std::vector<unsigned> searchableAttributeIsMultiValuedVector;
    std::vector<unsigned> searchableAttributeHighlightEnabled;

    std::map<std::string, unsigned> refiningAttributeNameToId;
    std::vector<FilterType> refiningAttributeTypeVector;
    std::vector<std::string> refiningAttributeDefaultValueVector;
    std::vector<bool> refiningAttributeIsMultiValuedVector;

    srch2::instantsearch::IndexType indexType;
    srch2::instantsearch::PositionIndexType positionIndexType;

    std::vector<unsigned> aclSearchableAttrIds;
    std::vector<unsigned> nonAclSearchableAttrIds;
    std::vector<unsigned> aclRefiningAttrIds;
    std::vector<unsigned> nonAclRefiningAttrIds;

    std::string nameOfLatitudeAttribute;
    std::string nameOfLongitudeAttribute;


    std::string scoringExpressionString;
    bool supportSwapInEditDistance;

    bool commited;

    friend class boost::serialization::access;

    template<class Archive>
    void serialize(Archive & ar, const unsigned int version) {
        ar & primaryKey;
        ar & scoringExpressionString;
        ar & searchableAttributeNameToId;
        ar & searchableAttributeTypeVector;
        ar & searchableAttributeBoostVector;
        ar & searchableAttributeIsMultiValuedVector;
        ar & refiningAttributeNameToId;
        ar & refiningAttributeTypeVector;
        ar & refiningAttributeDefaultValueVector;
        ar & refiningAttributeIsMultiValuedVector;
        ar & searchableAttributeHighlightEnabled;
        ar & indexType;
        ar & positionIndexType;
        ar & nameOfLatitudeAttribute;
        ar & nameOfLongitudeAttribute;
        ar & supportSwapInEditDistance;
        ar & aclSearchableAttrIds;
        ar & nonAclSearchableAttrIds;
        ar & aclRefiningAttrIds;
        ar & nonAclRefiningAttrIds;
    }
};

}
}

#endif //__SCHEMAINTERNAL_H__
