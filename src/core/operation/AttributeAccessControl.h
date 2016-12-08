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
 * AccessControl.h
 *
 *  Created on: Aug 18, 2014
 */

#ifndef __CORE_OPERATION_ACCESSCONTROL_H__
#define __CORE_OPERATION_ACCESSCONTROL_H__

#include <vector>
#include <string>
#include <map>
#include <sstream>
#include <boost/serialization/string.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/thread/shared_mutex.hpp>
#include "record/SchemaInternal.h"
#include "json/json.h"

namespace srch2 {
namespace instantsearch {

typedef boost::shared_mutex AttributeAclLock;
typedef boost::unique_lock< AttributeAclLock > AclWriteLock;
typedef boost::shared_lock< AttributeAclLock > AclReadLock;
using namespace std;

class PairOfAttrsListSharedPtr{
public:
	boost::shared_ptr<vector<unsigned> > searchableAttrList;
	boost::shared_ptr<vector<unsigned> > refiningAttrList;
	template<class Archive>
	void serialize(Archive & ar, const unsigned int version) {
		ar & searchableAttrList;
		ar & refiningAttrList;
	}
};

typedef map<string, PairOfAttrsListSharedPtr>::iterator AclMapIter;
typedef map<string, PairOfAttrsListSharedPtr>::const_iterator AclMapConstIter;

enum AclActionType {
	ACL_REPLACE, // insert new acl
	ACL_DELETE, // delete existing acl
	ACL_APPEND  // append to existing acl
};
class AttributeAccessControl {
public:
	AttributeAccessControl(const SchemaInternal *schema) {
		this->schema = schema;
	}
	// ----------------------------
	// read operations
	// ----------------------------

	// Get accessible searchable attributes for a given acl role name.
	void fetchSearchableAttrsAcl(const string& aclRoleValue, boost::shared_ptr<vector<unsigned> >& attrList) const;

	// Get accessible refining attributes for a given acl role name.
	void fetchRefiningAttrsAcl(const string& aclRoleValue, boost::shared_ptr<vector<unsigned> >& attrList) const;

	// ----------------------------
	// write operations
	// ----------------------------

	// Bulk load ACL for first time.
	// Thread unsafe. Should be called only from main thread during initial load.
	void bulkLoadAttributeAclCSV(const string& aclLoadFileName) const;

	void bulkLoadAttributeAclJSON(const std::string& aclLoadFileName) const;

	bool processSingleJSONAttributeAcl(const Json::Value& doc, AclActionType action,
			const string& apiName, Json::Value& aclAttributeResponse) const;

	// replace a new acl for a attribute
	void replaceFromAcl(vector<string>& aclRoleValue, vector<unsigned>& searchableAttrIdsList,
			vector<unsigned>& refiningAttrIdsList);

	// append to existing acl for a role. If role is not found then it is created.
	void appendToAcl(const string& aclRoleValue, const vector<unsigned>& searchableAttrIdsList,
			const vector<unsigned>& refiningAttrIdsList);

	// delete the attributes from existing acl for a role. If role is not found then it is ignored.
	void deleteFromAcl(const string& aclRoleValue, const vector<unsigned>& searchableAttrIdsList,
			const vector<unsigned>& refiningAttrIdsList);

	virtual ~AttributeAccessControl() {
	}

	void toString(stringstream& ss) const;

	// Helper function to validate whether searchable field is accessible for given role-id
	bool isSearchableFieldAccessibleForRole(const string& roleId, const string& fieldName) const;

	// Helper function to validate whether refining field is accessible for given role-id
	bool isRefiningFieldAccessibleForRole(const string& roleId, const string& fieldName) const;

private:
	mutable AttributeAclLock attrAclLock;
	// This is the data structure which stores the mapping from acl-role to
	// attributes accessible by this role. Attributes are stored as pair of searchable
	// and refining attribute lists.
	map<string, PairOfAttrsListSharedPtr > attributeAclMap;
	const SchemaInternal *schema;

	// private Helper functions

	//convert attribute names to attribute ids
	void convertFieldNamesToSortedFieldIds(vector<string>& fieldTokens,
			vector<unsigned>& searchableAttrIdsList, vector<unsigned>& refiningAttrIdsList) const;

	// Helper function to validate whether field is accessible for given role-id
	bool isFieldAccessibleForRole(const string& roleId, const string& fieldName,
			bool isFieldSearchable = true) const;

	// process acl request
	bool processAclRequest(vector<string>& fields, vector<string>& roleValues, AclActionType action) const;

	//Internal API which loads attribute acl JSON file. This API is called from wrapper API bulkLoadAttributeAclJSON
	void _bulkLoadAttributeAclJSON(const std::string& aclLoadFileName) const;

	//Internal API which loads attribute acl CSV file. This API is called from wrapper API bulkLoadAttributeAclCSV
	void _bulkLoadAttributeAclCSV(const std::string& aclLoadFileName) const;

    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version) {
    	ar & attributeAclMap;
    }
};

} /* namespace instantsearch */
} /* namespace srch2 */
#endif /* __CORE_OPERATION_ACCESSCONTROL_H__ */
