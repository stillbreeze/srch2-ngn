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
 * AccessControl.cpp
 *
 *  Created on: Aug 18, 2014
 */

#include "AttributeAccessControl.h"
#include "util/Logger.h"
#include "util/Assert.h"
#include <boost/algorithm/string.hpp>
#include "thirdparty/utf8/utf8.h"

using namespace srch2::util;

namespace srch2 {
namespace instantsearch {

/*
 *   This API loads acl csv file and should be called during engine's boot up.
 *   The format of ACL file is
 *   role_id , attribute1, attribute 2 , ....
 *   This API is exception safe.
 */
void  AttributeAccessControl::bulkLoadAttributeAclCSV(const std::string& aclLoadFileName) const{
	try {
		_bulkLoadAttributeAclCSV(aclLoadFileName);
	}catch(exception& exception) {
		Logger::error(exception.what());
		Logger::error("Attribute acl csv bulk load was not successful. Please check CSV file.");
	}
}
/*
 *   Internal API which loads acl CSV file. This API is called from wrapper API bulkLoadAttributeAclCSV
 */
void  AttributeAccessControl::_bulkLoadAttributeAclCSV(const std::string& aclLoadFileName) const{

	if (aclLoadFileName == "") {
		if (schema->getAclRefiningAttrIdsList().size() > 0 ||
				schema->getAclSearchableAttrIdsList().size() > 0) {
			// if there are fields in schema with acl enabled. Then print warning
			// that bulk load file is not specified.
			Logger::warn("Attribute acl CSV file for bulk load is not specified!. "
					"Please use Attribute Acl REST APIs for adding permissions");
		}
		return;
	}

	std::ifstream input(aclLoadFileName.c_str());
	if (!input.good()) {
		Logger::warn("The attribute acl CSV file = \"%s\" could not be opened.",
				aclLoadFileName.c_str());
		return;
	}
	Logger::console("Loading attributes CSV acl file %s", aclLoadFileName.c_str());
	std::string line;
	unsigned lineCount = 0;
	while (getline(input, line)) {
		++lineCount;
		boost::algorithm::trim(line);
		if (line.size() == 0)  // ignore empty line
			continue;

		if (line[0] == '#') // ignore comment
			continue;

		std::vector<std::string> tokens;
		boost::algorithm::split(tokens, line, boost::is_any_of(","));
		if (tokens.size() < 2){ // if attributes are not specified then skip this row.
			Logger::warn("ACL: invalid row format found in row = %d of access control file. Ignoring this line", lineCount);
			continue;
		}

		std::string roleValue = tokens[0];
		if (roleValue.size() == 0){ // if role id is not specified then skip this row.
			Logger::warn("ACL: empty role-id value in row = %d of access control file. Ignoring this line.", lineCount);
			continue;
		}

		tokens.erase(tokens.begin());

		std::vector<unsigned> searchableAttrIdsList;
		std::vector<unsigned> refiningAttrIdsList;
		convertFieldNamesToSortedFieldIds(tokens, searchableAttrIdsList, refiningAttrIdsList);

		// if searchableAttrIdsList and refiningAttrIdsList are empty then skip this row
		// this can happen when all field names mentioned in the row are bogus.
		if (searchableAttrIdsList.size() == 0 && refiningAttrIdsList.size() == 0) {
			Logger::warn("ACL: No valid attributes found in row = %d of access control file. Ignoring this line.", lineCount);
			continue;
		}
		// If everything looks fine then Append to ACL. Note: Append becomes insert if the role is not
		// present in acl map. If the role is present then append to existing role instead of overwriting it.
		const_cast<AttributeAccessControl *>(this)->appendToAcl(roleValue, searchableAttrIdsList, refiningAttrIdsList);
	}

}
/*
 *   This API loads acl JSON file and should be called during engine's boot up.
 *   The format of ACL file is
 *   { "roleId" : ["role_id1", "role_id2"] , attributes : ["attribute1", "attribute 2" , .... ] }
 *   This API is exception safe.
 */
void  AttributeAccessControl::bulkLoadAttributeAclJSON(const std::string& aclLoadFileName) const{
	try{
		_bulkLoadAttributeAclJSON(aclLoadFileName);
	} catch (exception& ex) {
		Logger::error(ex.what());
		Logger::error("Attribute acl JSON bulk load was not successful. Please check JSON file.");
	}
}
/*
 *   Internal API which loads acl JSON file. This API is called from wrapper API bulkLoadAttributeAclJSON
 */
void  AttributeAccessControl::_bulkLoadAttributeAclJSON(const std::string& aclLoadFileName) const{
	if (aclLoadFileName == "") {
		if (schema->getAclRefiningAttrIdsList().size() > 0 ||
				schema->getAclSearchableAttrIdsList().size() > 0) {
			// if there are fields in schema with acl enabled. Then print warning
			// that bulk load file is not specified.
			Logger::warn("Attribute acl JSON file for bulk load is not specified!. "
					"Please use Attribute Acl REST APIs for adding permissions");
		}
		return;
	}

	std::ifstream input(aclLoadFileName.c_str());
	if (!input.good()) {
		Logger::warn("The attribute acl JSON file = \"%s\" could not be opened.",
				aclLoadFileName.c_str());
		return;
	}
	Logger::console("Loading attributes acl JSON file %s", aclLoadFileName.c_str());
	std::string line;
	unsigned lineCount = 0;
	bool isArrayOfJsonRecords = false;
	while (getline(input, line)) {
		++lineCount;
		boost::algorithm::trim(line);
		if (line.size() == 0)  // ignore empty line
			continue;
		if (line[0] == '#') // ignore comment
			continue;

		if (line == "[") {
			// begin of JSON array in file
			isArrayOfJsonRecords = true;
			continue; // ignore
		}
		if (isArrayOfJsonRecords == true && line == "]") {
			// end of JSON array in file
			break; // assume nothing follows array (will ignore more records or another array)
		}

		string::const_iterator end_it = utf8::find_invalid(line.begin(), line.end());
		if (end_it != line.end()) {
			stringstream ss;
			ss << "Invalid UTF-8 encoding detected at line " << lineCount << ". Ignoring this line";
			Logger::warn("%s", ss.str().c_str());
			continue;  // ignore this line
		}

		Json::Value doc;
		Json::Reader reader;
		bool parseSuccess = reader.parse(line, doc, false);
		if (!parseSuccess)
		{
			stringstream ss;
			ss << "Failed to parse JSON - " << reader.getFormatedErrorMessages();
			Logger::warn("%s", ss.str().c_str());
			continue; // ignore this line
		}
		Json::Value response;
		processSingleJSONAttributeAcl(doc, ACL_APPEND, "bulkLoad", response);
		if (response.type() == Json::stringValue && response.asString().size() > 0) {
			Logger::info(response.asCString());
		}
	}
}
/*
 *   Helper API to handle a single ACL operation. (insert, delete, or append)
 */
bool AttributeAccessControl::processSingleJSONAttributeAcl(const Json::Value& doc, AclActionType action,
		const string& apiName, Json::Value& aclAttributeResponse) const{

	Json::Value attributesToAdd = doc.get("attributes", Json::Value(Json::arrayValue));
	Json::Value attributesRoles = doc.get("roleId", Json::Value(Json::arrayValue));

	if (attributesToAdd.type()  != Json::arrayValue) {
		std::stringstream log_str;
		log_str << "API : " << apiName << ", Error: 'attributes' key is not an array in request JSON.";
		aclAttributeResponse = log_str.str();
		return false;
	}
	if (attributesToAdd.size() == 0) {
		std::stringstream log_str;
		log_str << "API : " << apiName << ", Error: 'attributes' key is empty or missing in request JSON.";
		aclAttributeResponse = log_str.str();
		return false;
	}
	if (attributesRoles.type() != Json::arrayValue) {
		std::stringstream log_str;
		log_str << "API : " << apiName << ", Error: 'roleId' key is not an array in request JSON.";
		aclAttributeResponse = log_str.str();
		return false;
	}
	if (attributesRoles.size() == 0) {
		std::stringstream log_str;
		log_str << "API : " << apiName << ", Error: 'roleId' key is empty or missing in request JSON.";
		aclAttributeResponse = log_str.str();
		return false;
	}

	vector<string> attributeList;
	vector<string> invalidAttributeNames;
	for (unsigned i = 0; i < attributesToAdd.size(); ++i) {
		Json::Value defaultValueToReturn = Json::Value("");
		const Json::Value attribute = attributesToAdd.get(i, defaultValueToReturn);
		if (attribute.type() != Json::stringValue){
			std::stringstream log_str;
			log_str << "API : " << apiName << ", Error: 'attributes' key's element at index "<< i << " is not convertible to string";
			aclAttributeResponse = log_str.str();
			return false;
		}
		string tempString = attribute.asString();
		boost::algorithm::trim(tempString);
		if (tempString.size() != 0) {
			if (tempString == "*" || schema->isValidAttribute(tempString)) {
				attributeList.push_back(tempString);
			} else {
				invalidAttributeNames.push_back(tempString);
			}
		}
	}

	if (attributeList.size() == 0) {
		// All elements in the attribute list are either empty or have bogus value.
		std::stringstream log_str;
		log_str << "API : " << apiName << ", Error: 'attributes' key's elements are not valid.";
		aclAttributeResponse = log_str.str();
		return false;
	}

	// We have some valid attribute names in attributes list. Check whether there are some invalid
	// name as well. If there are invalid attribute names, then generate warning log message and proceed
	if (invalidAttributeNames.size() > 0) {
		std::stringstream log_str;
		if (invalidAttributeNames.size() > 1)
			log_str << "API : " << apiName << ", Warning: 'attributes' key has bad attributes = '";
		else
			log_str << "API : " << apiName << ", Warning: 'attributes' key has bad attribute = '";
		for (unsigned i = 0; i < invalidAttributeNames.size(); ++i) {
			if (i)
				log_str << ", ";
			log_str << invalidAttributeNames[i];
		}
		log_str << "'.";
		aclAttributeResponse = log_str.str();
	}

	vector<string> roleIds;
	for (unsigned i = 0; i < attributesRoles.size(); ++i) {
		Json::Value defaultValueToReturn = Json::Value("");
		const Json::Value roleId = attributesRoles.get(i, defaultValueToReturn);

		switch (roleId.type()) {
		case Json::stringValue:
		{
			string tempString = roleId.asString();
			boost::algorithm::trim(tempString);
			if (tempString.size() != 0)
				roleIds.push_back(tempString);
			break;
		}
		case Json::intValue:
		{
			// convert int to string instead of returning error to user
			stringstream tempString;
			tempString << roleId.asInt64();
			roleIds.push_back(tempString.str());
			break;
		}
		case Json::uintValue:
		{
			// convert unsigned int to string instead of returning error to user
			stringstream tempString;
			tempString << roleId.asUInt64();
			roleIds.push_back(tempString.str());
			break;
		}
		case Json::realValue:
		{
			// convert double to string instead of returning error to user
			stringstream tempString;
			tempString << roleId.asDouble();
			roleIds.push_back(tempString.str());
			break;
		}
		case Json::arrayValue:
		case Json::objectValue:
		{
			// Can't convert to array ..user should fix the input JSON.
			std::stringstream log_str;
			log_str << "API : " << apiName << ", Error: 'roleId' key's element at index "<< i << " is not convertible to string";
			aclAttributeResponse = log_str.str();
			return false;
		}
		default:
			ASSERT(false);
		}
	}

	if (roleIds.size() == 0) {
		std::stringstream log_str;
		log_str << "API : " << apiName << ", Error: 'roleId' key's elements are not valid.";
		aclAttributeResponse = log_str.str();
		return false;
	}

	return processAclRequest(attributeList, roleIds, action);
}
/*
 *   This API converts attribute names to attribute ids and return sorted attribute ids.
 */
void AttributeAccessControl::convertFieldNamesToSortedFieldIds(vector<string>& fieldTokens,
		vector<unsigned>& searchableAttrIdsList, vector<unsigned>& refiningAttrIdsList) const{
	for (unsigned i = 0; i < fieldTokens.size(); ++i) {
		if (fieldTokens[i].size() == 0)
			continue;
		boost::algorithm::trim(fieldTokens[i]);

		// if one of the fields is "*" then return all the attributes (refining+searchable) with
		// ACL flag. Non-Acl attributes are always accessible, so no need to include them.
		if (fieldTokens[i] == "*") {
			searchableAttrIdsList.clear();
			refiningAttrIdsList.clear();
			searchableAttrIdsList = schema->getAclSearchableAttrIdsList();
			refiningAttrIdsList = schema->getAclRefiningAttrIdsList();
			return;
		}
		int id = schema->getSearchableAttributeId(fieldTokens[i]);
		if (id != -1) {
			searchableAttrIdsList.push_back(id);
		} else {
			id = schema->getRefiningAttributeId(fieldTokens[i]);
			if (id != -1) {
				refiningAttrIdsList.push_back(id);
			} else {
				Logger::warn("ACL: invalid attribute name = '%s' in field list",
						fieldTokens[i].c_str());
			}
		}
	}
	std::sort(searchableAttrIdsList.begin(), searchableAttrIdsList.end());
	std::sort(refiningAttrIdsList.begin(), refiningAttrIdsList.end());
}

/*
 *  This API processes the  ACL operations such as insertion, deletion, and append.
 *  The inputs are list of attributes, list of role-ids, and operation to be performed.
 *
 *  e.g.
 *  attributes: [f1 , f2, f3]
 *  role-ids : [100, 101]
 *  action : insert
 *
 *  then the API performs following inserts
 *
 *  100 -> [ f1 , f2, f3 ]
 *  101 -> [ f1 , f2, f3 ]
 *
 */
bool  AttributeAccessControl::processAclRequest( vector<string>& fieldTokens,
		vector<string>& roleValueTokens, AclActionType action) const{

	if (fieldTokens.size() == 0)
		return false;

	std::vector<unsigned> searchableAttrIdsList;
	std::vector<unsigned> refiningAttrIdsList;

	convertFieldNamesToSortedFieldIds(fieldTokens, searchableAttrIdsList, refiningAttrIdsList);

	if (roleValueTokens.size() == 0)
		return false;

	// Loop over all the role-ids and perform required operations for the list of attributes

		switch(action) {
		case ACL_REPLACE:
		{
			const_cast<AttributeAccessControl *>(this)->replaceFromAcl(roleValueTokens, searchableAttrIdsList, refiningAttrIdsList);
			break;
		}
		case ACL_DELETE:
		{
			for (unsigned i = 0; i < roleValueTokens.size(); ++i) {
				// delete from ACL
				boost::algorithm::trim(roleValueTokens[i]);
				const_cast<AttributeAccessControl *>(this)->deleteFromAcl(roleValueTokens[i], searchableAttrIdsList, refiningAttrIdsList);
			}
			break;
		}
		case ACL_APPEND:
		{
			for (unsigned i = 0; i < roleValueTokens.size(); ++i) {
				// append to existing ACL. If acl-role is not found then add it.
				boost::algorithm::trim(roleValueTokens[i]);
				const_cast<AttributeAccessControl *>(this)->appendToAcl(roleValueTokens[i], searchableAttrIdsList, refiningAttrIdsList);
			}
			break;
		}
		default:
			ASSERT(false);
		}

	return true;
}

/*
 *   This API insert attributes to a given roleId into the acl map. If the role-id exists
 *   then it will be overwritten.
 */
void AttributeAccessControl::replaceFromAcl(vector<string>& roleValueTokens, vector<unsigned>& searchableAttrIdsList,
		vector<unsigned>& refiningAttrIdsList) {
	AclWriteLock lock(attrAclLock);  // X-lock
	// replace operation consists of two steps.
	// 1. delete attribute from all roldIds present in the map but are not in the input roleIds
	// 2. append attributes for the input roleIds
	AclMapIter iter = attributeAclMap.begin();
	while(iter != attributeAclMap.end()) {

		// first check whether any of the input searchable attributes are present in the allowed
		// searchable attributes for this role-id. If present delete the matching attributes using
		// set_difference. If not present then skip.
		vector<unsigned>::iterator matchedIter = find_first_of (iter->second.searchableAttrList->begin(), iter->second.searchableAttrList->end(),
				searchableAttrIdsList.begin(), searchableAttrIdsList.end());

		if (matchedIter != iter->second.searchableAttrList->end()) {
			vector<unsigned> *attrListPtr = new vector<unsigned>();
			attrListPtr->reserve(searchableAttrIdsList.size() + iter->second.searchableAttrList->size());
			std::set_difference(iter->second.searchableAttrList->begin(), iter->second.searchableAttrList->end(),
					searchableAttrIdsList.begin(), searchableAttrIdsList.end(),
					back_inserter(*attrListPtr));

			iter->second.searchableAttrList.reset(attrListPtr);
		}

		// first check whether any of the input refining attributes are present in the allowed
		// refining attributes for this role-id. If present delete the matching attributes using
		// set_difference. If not present then skip.
		matchedIter = find_first_of(iter->second.refiningAttrList->begin(), iter->second.refiningAttrList->end(),
				refiningAttrIdsList.begin(), refiningAttrIdsList.end());
		if (matchedIter != iter->second.refiningAttrList->end()) {
			vector<unsigned> *attrListPtr = new vector<unsigned>();
			attrListPtr->reserve(refiningAttrIdsList.size() + iter->second.refiningAttrList->size());
			std::set_difference(iter->second.refiningAttrList->begin(), iter->second.refiningAttrList->end(),
					refiningAttrIdsList.begin(), refiningAttrIdsList.end(),
					back_inserter(*attrListPtr));
			iter->second.refiningAttrList.reset(attrListPtr);
		}

		// if all the attributes are deleted then remove the acl role from map as well.
		if (iter->second.searchableAttrList->size() == 0 && iter->second.refiningAttrList->size() == 0)
			attributeAclMap.erase(iter++);
		else
			++iter;
	}
	lock.unlock();  // append ACL below acquires the X lock hence unlock here to avoid deadlock.

	for (unsigned i = 0; i < roleValueTokens.size(); ++i) {
		// append to existing ACL. If acl-role is not found then add it.
		boost::algorithm::trim(roleValueTokens[i]);
		const_cast<AttributeAccessControl *>(this)->appendToAcl(roleValueTokens[i], searchableAttrIdsList, refiningAttrIdsList);
	}
}

/*
 *  This API merges the attributes to an existing acl for a give role-id. If role-id is not found
 *  then it is created.
 *  e.g
 *  existing acl for 101
 *  "101" -> [ f1, f2, f3 ]
 *
 *  input acl for "101" -> [ f2, f7 ]
 *
 *  final acl for "101" -> [f1, f2, f3, f7]
 */
void AttributeAccessControl::appendToAcl(const string& aclRoleValue, const vector<unsigned>& searchableAttrIdsList,
		const vector<unsigned>& refiningAttrIdsList) {
	AclWriteLock lock(attrAclLock); // X-lock
	AclMapIter iter = attributeAclMap.find(aclRoleValue);
	if (iter != attributeAclMap.end()) {
		// if role-id is found then merge the existing attributes list with the new attributes
		// list into a new vector. The new vector will have unique attributes in a sorted order.
		// Then replace the old (existing attributes) vector with the new vector. The old vector will
		// be free'd automatically by shared_ptr after the last reader.
		vector<unsigned> *attrListPtr = new vector<unsigned>();
		attrListPtr->reserve(searchableAttrIdsList.size() + iter->second.searchableAttrList->size());
		std::set_union(iter->second.searchableAttrList->begin(), iter->second.searchableAttrList->end(),
				searchableAttrIdsList.begin(), searchableAttrIdsList.end(),
				back_inserter(*attrListPtr));
		iter->second.searchableAttrList.reset(attrListPtr);

		attrListPtr = new vector<unsigned>();
		attrListPtr->reserve(refiningAttrIdsList.size() + iter->second.refiningAttrList->size());
		std::set_union(iter->second.refiningAttrList->begin(), iter->second.refiningAttrList->end(),
				refiningAttrIdsList.begin(), refiningAttrIdsList.end(),
				back_inserter(*attrListPtr));
		iter->second.refiningAttrList.reset(attrListPtr);
		return;
	} else {
		// if not found then insert new entry.
		PairOfAttrsListSharedPtr newAttrListPair;
		newAttrListPair.searchableAttrList.reset(new vector<unsigned>());
		newAttrListPair.refiningAttrList.reset(new vector<unsigned>());
		pair<AclMapIter, bool> ret =
		attributeAclMap.insert(make_pair(aclRoleValue,newAttrListPair));
		ret.first->second.searchableAttrList->insert(ret.first->second.searchableAttrList->end(),
				searchableAttrIdsList.begin(), searchableAttrIdsList.end());
		ret.first->second.refiningAttrList->insert(ret.first->second.refiningAttrList->end(),
				refiningAttrIdsList.begin(), refiningAttrIdsList.end());
		return;
	}
}

/*
 *  This API deletes the attributes from existing acl for a given role-id. If role-id is not found
 *  then it is ignored.
 *
 *  e.g
 *  existing acl for 101
 *  101 -> [ f1, f2, f3 ]
 *
 *  input acl for 101 -> [ f2, f7 ]
 *
 *  final acl for 101 -> [f1, f3]
 *
 *  Note: if not attributes are left in acl for a role-id then that role-id will be removed from
 *  the acl.
 */
void AttributeAccessControl::deleteFromAcl(const string& aclRoleValue, const vector<unsigned>& searchableAttrIdsList,
		const vector<unsigned>& refiningAttrIdsList) {
	AclWriteLock lock(attrAclLock); // X-lock
	AclMapIter iter = attributeAclMap.find(aclRoleValue);
	if (iter != attributeAclMap.end()) {
		// if role-id is found then copy the difference of existing attributes list and to be
		// delete attribute list into a new vector. The new vector will have the unique attributes
		// in a sorted order. Then replace the old (existing attributes) vector with the new vector.
		// The old vector will be free'd automatically by shared_ptr after the last reader.
		vector<unsigned> *attrListPtr = new vector<unsigned>();
		attrListPtr->reserve(searchableAttrIdsList.size() + iter->second.searchableAttrList->size());
		std::set_difference(iter->second.searchableAttrList->begin(), iter->second.searchableAttrList->end(),
				searchableAttrIdsList.begin(), searchableAttrIdsList.end(),
				back_inserter(*attrListPtr));

		iter->second.searchableAttrList.reset(attrListPtr);

		attrListPtr = new vector<unsigned>();
		attrListPtr->reserve(refiningAttrIdsList.size() + iter->second.refiningAttrList->size());
		std::set_difference(iter->second.refiningAttrList->begin(), iter->second.refiningAttrList->end(),
				refiningAttrIdsList.begin(), refiningAttrIdsList.end(),
				back_inserter(*attrListPtr));
		iter->second.refiningAttrList.reset(attrListPtr);

		// if all the attributes are deleted then remove the acl role from map as well.
		if (iter->second.searchableAttrList->size() == 0 && iter->second.refiningAttrList->size() == 0)
			attributeAclMap.erase(iter);
		return;
	}
}

/*
 *  This API fetches accessible searchable attributes for a given acl role-id.
 */
void AttributeAccessControl::fetchSearchableAttrsAcl(const string& aclRoleValue, boost::shared_ptr<vector<unsigned> >& attrList) const{
	AclReadLock lock(attrAclLock); // shared-lock
	AclMapConstIter iter = attributeAclMap.find(aclRoleValue);
	if (iter != attributeAclMap.end()) {
		attrList = iter->second.searchableAttrList;
	} else {
		attrList.reset();
	}
}

/*
 *  This API fetches accessible refining attributes for a given acl role-id.
 */
void AttributeAccessControl::fetchRefiningAttrsAcl(const string& aclRoleValue, boost::shared_ptr<vector<unsigned> >& attrList) const{
	AclReadLock lock(attrAclLock); // shared-lock
	AclMapConstIter iter = attributeAclMap.find(aclRoleValue);
	if (iter != attributeAclMap.end()) {
		attrList = iter->second.refiningAttrList;
	} else {
		attrList.reset();
	}
}

/*
 *   Helper function to validate whether searchable field is accessible for given role-id
 */
bool AttributeAccessControl::isSearchableFieldAccessibleForRole(const string& roleId,
		const string& fieldName) const{
	// The last argument is true to indicate that the field is searchable.
	return isFieldAccessibleForRole(roleId, fieldName, true);
}

/*
 *   Helper function to validate whether refining field is accessible for given role-id
 */
bool AttributeAccessControl::isRefiningFieldAccessibleForRole(const string& roleId,
		const string& fieldName) const{
	// The last argument is false to indicate that the field is refining.
	return isFieldAccessibleForRole(roleId, fieldName, false);
}

/*
 *   Helper function to validate whether field is accessible for given role-id
 */
bool AttributeAccessControl::isFieldAccessibleForRole(const string& roleId,
		const string& fieldName, bool isfieldSearchable) const{

	unsigned fieldId;
	vector<unsigned> schemaNonAclAttrsList;
	boost::shared_ptr<vector<unsigned> > allowedAttributesSharedPtr;

	if (isfieldSearchable) {
		// get field id for fieldname
		fieldId = schema->getSearchableAttributeId(fieldName);
		// fetch public (non-acl) searchable field
		schemaNonAclAttrsList = schema->getNonAclSearchableAttrIdsList();
		// Fetch list of attributes accessible by this role-id
		fetchSearchableAttrsAcl(roleId, allowedAttributesSharedPtr);


	} else {
		// get field id for fieldname
		fieldId = schema->getRefiningAttributeId(fieldName);
		// fetch public (non-acl) searchable field
		schemaNonAclAttrsList = schema->getNonAclRefiningAttrIdsList();
		// Fetch list of attributes accessible by this role-id
		fetchRefiningAttrsAcl(roleId, allowedAttributesSharedPtr);
	}

	vector<unsigned> *allowedAttributesForRole = allowedAttributesSharedPtr.get();

	// if the filed name is present in a list of attributes for a given roleId OR field is public
	// field ( non-acl), then it is accessible field otherwise it is unaccessible.
	bool accessible = (allowedAttributesForRole != NULL &&
			find(allowedAttributesForRole->begin(), allowedAttributesForRole->end(), fieldId)
				!= allowedAttributesForRole->end()) ||
			(find(schemaNonAclAttrsList.begin(), schemaNonAclAttrsList.end(), fieldId)
				!= schemaNonAclAttrsList.end());;

	return accessible;
}

// This function serializes the access control datastructure to string. This API is
// used for debugging.
void AttributeAccessControl::toString(stringstream& ss) const{
	AclMapConstIter iter = attributeAclMap.begin();
	while(iter != attributeAclMap.end()) {
		ss << iter->first << " = S : [ ";
		for (unsigned i = 0; i < iter->second.searchableAttrList->size(); ++i) {
			ss << iter->second.searchableAttrList->operator [](i) << " ";
		}
		ss << "], R : [ ";
		for (unsigned i = 0; i < iter->second.refiningAttrList->size(); ++i) {
			ss << iter->second.refiningAttrList->operator [](i) << " ";
		}
		ss << "]\n";
		++iter;
	}
}

} /* namespace instantsearch */
} /* namespace srch2 */
