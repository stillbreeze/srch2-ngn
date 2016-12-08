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
#include "IndexWriteUtil.h"

using namespace srch2::httpwrapper;

namespace {
const char* c_action_insert = "insert";
const char* c_action_delete = "delete";
const char* c_action_update = "update";
const char* c_action_save = "save";
const char* c_action_export = "export";
const char* c_action_commit = "commit";
const char* c_action_acl = "acl";

const char* c_rid = "rid";
const char* c_reason = "reason";
const char* c_detail = "details";
const char* c_resume = "resume";

const char* c_failed = "failed";
const char* c_success = "success";
}

Json::Value IndexWriteUtil::_insertCommand(Indexer *indexer,
		const CoreInfo_t *indexDataContainerConf, const Json::Value &root,
		Record *record) {
	Json::Value response(Json::objectValue);

	Schema * storedSchema = Schema::create();
	RecordSerializerUtil::populateStoredSchema(storedSchema,
			indexer->getSchema());
	RecordSerializer recSerializer = RecordSerializer(*storedSchema);

	std::stringstream errorStream;
	if (!JSONRecordParser::_JSONValueObjectToRecord(record, root,
			indexDataContainerConf, errorStream, recSerializer)) {
		response[c_rid] = record->getPrimaryKey();
		response[c_action_insert] = c_failed;
		response[c_reason] =
				"parse: The record is not in a correct json format";
		response[c_detail] = errorStream.str();

		delete storedSchema;
		return response;
	}
	//add the record to the index

	if (indexer->getNumberOfDocumentsInIndex()
			< indexDataContainerConf->getDocumentLimit()) {
		// Do NOT delete analyzer because it is thread specific. It will be reused for
		// search/update/delete operations.
		srch2::instantsearch::Analyzer * analyzer =
				AnalyzerFactory::getCurrentThreadAnalyzerWithSynonyms(
						indexDataContainerConf);
		srch2::instantsearch::INDEXWRITE_RETVAL ret = indexer->addRecord(record,
				analyzer);

		switch (ret) {
		case srch2::instantsearch::OP_SUCCESS: {
			response[c_rid] = record->getPrimaryKey();
			response[c_action_insert] = c_success;
			break;
		}
		case srch2::instantsearch::OP_FAIL: {
			response[c_rid] = record->getPrimaryKey();
			response[c_action_insert] = c_failed;
			response[c_reason] =
					"The record with same primary key already exists";
			break;
		}
		};
	} else {
		response[c_rid] = record->getPrimaryKey();
		response[c_action_insert] = c_failed;
		response[c_reason] =
				"document limit reached. Email support@srch2.com for account upgrade.";
	}
	delete storedSchema;
	return response;
}

Json::Value IndexWriteUtil::_deleteCommand(Indexer *indexer,
		const CoreInfo_t *indexDataContainerConf, const Json::Value &root) {
	Json::Value response(Json::objectValue);
	//set the primary key of the record we want to delete
	std::string primaryKeyName = indexDataContainerConf->getPrimaryKey();
	const std::string primaryKeyStringValue =
			root.get(primaryKeyName, "NULL").asString();

	response[c_rid] = primaryKeyStringValue;

	if (primaryKeyStringValue.compare("NULL") != 0) {
		//delete the record from the index

		switch (indexer->deleteRecord(primaryKeyStringValue)) {
		case OP_FAIL: {
			response[c_action_delete] = c_failed;
			response[c_reason] = "no record with given primary key";
			break;
		}
		default: // OP_SUCCESS.
		{
			response[c_action_delete] = c_success;
		}
		};
	} else {
		response[c_action_delete] = c_failed;
		response[c_reason] = "no record with given primary key";
	}
	return response;
}

Json::Value IndexWriteUtil::_deleteCommand_QueryURI(Indexer *indexer,
		const CoreInfo_t *indexDataContainerConf, const evkeyvalq &headers) {
	Json::Value response(Json::objectValue);
	//set the primary key of the record we want to delete
	std::string primaryKeyName = indexDataContainerConf->getPrimaryKey();

	const char *pKeyParamName = evhttp_find_header(&headers,
			primaryKeyName.c_str());

	if (pKeyParamName) {
		size_t sz;
		char *pKeyParamName_cstar = evhttp_uridecode(pKeyParamName, 0, &sz);

		//std::cout << "[" << termBoostsParamName_cstar << "]" << std::endl;
		const std::string primaryKeyStringValue = string(pKeyParamName_cstar);
		free(pKeyParamName_cstar);

		response[c_rid] = primaryKeyStringValue;

		//delete the record from the index
		switch (indexer->deleteRecord(primaryKeyStringValue)) {
		case OP_FAIL: {
			response[c_action_delete] = c_failed;
			response[c_reason] = "no record with given primary key";
			break;
		}
		default: // OP_SUCCESS.
		{
			response[c_action_delete] = c_success;
		}
		};
	} else {
		response[c_rid] = "NULL";
		response[c_action_delete] = c_failed;
		response[c_reason] = "no record with given primary key";
	}
	return response;
}

Json::Value IndexWriteUtil::_updateCommand(Indexer *indexer,
		const CoreInfo_t *indexDataContainerConf, const evkeyvalq &headers,
		const Json::Value &root, Record *record) {
	Json::Value response(Json::objectValue);
	/// step 1, delete old record

	//set the primary key of the record we want to delete
	std::string primaryKeyName = indexDataContainerConf->getPrimaryKey();

	unsigned deletedInternalRecordId;
	std::string primaryKeyStringValue;

	Schema * storedSchema = Schema::create();
	RecordSerializerUtil::populateStoredSchema(storedSchema,
			indexer->getSchema());
	RecordSerializer recSerializer = RecordSerializer(*storedSchema);

	std::stringstream errorStream;
	bool parseJson = JSONRecordParser::_JSONValueObjectToRecord(record, root,
			indexDataContainerConf, errorStream, recSerializer);
	delete storedSchema;
	if (!parseJson) {
		response[c_rid] = record->getPrimaryKey();
		response[c_action_update] = c_failed;
		response[c_reason] =
				"parse: The record is not in a correct json format";
		response[c_detail] = errorStream.str();
		return response;
	}

	primaryKeyStringValue = record->getPrimaryKey();
	response[c_rid] = primaryKeyStringValue;

	//delete the record from the index
	bool recordExisted = false;
	switch (indexer->deleteRecordGetInternalId(primaryKeyStringValue,
			deletedInternalRecordId)) {
	case srch2::instantsearch::OP_FAIL: {
		// record to update doesn't exit, will insert it
		break;
	}
	default: // OP_SUCCESS.
	{
		recordExisted = true;
	}
	};

	/// step 2, insert new record

	//add the record to the index

	if (indexer->getNumberOfDocumentsInIndex()
			< indexDataContainerConf->getDocumentLimit()) {
		// Do NOT delete analyzer because it is thread specific. It will be reused for
		// search/update/delete operations.
		Analyzer* analyzer = AnalyzerFactory::getCurrentThreadAnalyzer(
				indexDataContainerConf);
		srch2::instantsearch::INDEXWRITE_RETVAL ret = indexer->addRecord(record,
				analyzer);
		switch (ret) {
		case srch2::instantsearch::OP_SUCCESS: {
			if (recordExisted) {
				response[c_action_update] =
						"Existing record updated successfully";
			} else {
				response[c_action_update] = "New record inserted successfully";
			}
			return response;
		}
		case srch2::instantsearch::OP_FAIL: {
			response[c_action_update] = c_failed;
			response[c_reason] =
					"insert: The record with same primary key already exists";
			break;
		}
		};
	} else {
		response[c_action_update] = c_failed;
		response[c_reason] =
				"insert: Document limit reached. Email support@srch2.com for account upgrade.";
	}

	/// reaching here means the insert failed, need to resume the deleted old record

	srch2::instantsearch::INDEXWRITE_RETVAL ret = indexer->recoverRecord(
			primaryKeyStringValue, deletedInternalRecordId);

	switch (ret) {
	case srch2::instantsearch::OP_FAIL: {
		response[c_resume] = "no record with given primary key";
		break;
	}
	default: // OP_SUCCESS.
	{
		response[c_resume] = c_success;
	}
	};
	return response;
}

Json::Value IndexWriteUtil::_saveCommand(Indexer *indexer) {
	Json::Value response(Json::objectValue);
	indexer->save();
	response[c_action_save] = c_success;
	return response;
}

// save the exported data to exported_data.json
Json::Value IndexWriteUtil::_exportCommand(Indexer *indexer,
		const char* exportedDataFileName) {
	Json::Value response(Json::objectValue);
	indexer->exportData(exportedDataFileName);
	response[c_action_export] = c_success;
	return response;
}

Json::Value IndexWriteUtil::_commitCommand(Indexer *indexer,
		const CoreInfo_t *indexDataContainerConf, const uint64_t offset) {
	Json::Value response(Json::objectValue);
	//commit the index.
	if (indexer->commit() == srch2::instantsearch::OP_SUCCESS) {
		response[c_action_commit] = c_success;

		// do not save indexes to disk since we can always rebuild them from
		// indexer->save();
	} else {
		response[c_action_commit] = c_failed;
	}
	return response;
}

Json::Value IndexWriteUtil::_aclRecordModifyRoles(Indexer *indexer,
		string &primaryKeyID, vector<string> &roleIds,
		srch2::instantsearch::RecordAclCommandType commandType) {

	Json::Value response(Json::objectValue);
	srch2::instantsearch::INDEXWRITE_RETVAL returnValue = indexer->aclRecordModifyRoles(
			primaryKeyID, roleIds, commandType);

	switch (commandType) {
	case srch2::instantsearch::Acl_Record_Add:
		switch (returnValue) {
		case srch2::instantsearch::OP_SUCCESS:
			response[c_action_acl] = c_success;
			response[c_detail] = " Role id added successfully";
			break;
		case srch2::instantsearch::OP_FAIL:
			response[c_action_acl] = c_failed;
			response[c_detail] =
					"rid: " + primaryKeyID
							+ " add role failed. reason: No record with this primary key";
			break;
		}
		;
		break;
	case srch2::instantsearch::Acl_Record_Append:
		switch (returnValue) {
		case srch2::instantsearch::OP_SUCCESS:
			response[c_action_acl] = c_success;
			response[c_detail] = " Role id appended successfully";
			break;
		case srch2::instantsearch::OP_FAIL:
			response[c_action_acl] = c_failed;
			response[c_detail] =
					"rid: " + primaryKeyID
							+ " append role failed. reason: No record with this primary key";
			break;
		}
		;
		break;
	case srch2::instantsearch::Acl_Record_Delete:
		switch (returnValue) {
		case srch2::instantsearch::OP_SUCCESS:
			response[c_action_acl] = c_success;
			response[c_detail] = " Role id deleted successfully";
			break;
		case srch2::instantsearch::OP_FAIL:
			response[c_action_acl] = c_failed;
			response[c_detail] =
					"rid: " + primaryKeyID
							+ " delete role failed. reason: No record with this primary key";
			break;
		}
		;
		break;
	default:
		ASSERT(false);
		break;
	};
	return response;
}


Json::Value IndexWriteUtil::_aclModifyRecordsOfRole(Indexer *indexer, string &roleId,
		vector<string> &resourceIds, srch2::instantsearch::RecordAclCommandType commandType) {

	Json::Value response(Json::objectValue);
	if(commandType == Acl_Record_Add){
		indexer->deleteRoleRecord(roleId);
		commandType = Acl_Record_Append;
	}
	vector<string> roleIds;
	roleIds.push_back(roleId);
	srch2::instantsearch::INDEXWRITE_RETVAL returnValue;
	for(unsigned i = 0 ; i < resourceIds.size(); ++i){
		returnValue = indexer->aclRecordModifyRoles(resourceIds[i], roleIds, commandType);
		switch(returnValue){
		case srch2::instantsearch::OP_SUCCESS:
			break;
		case srch2::instantsearch::OP_FAIL:
			"No record with this primary key: " + resourceIds[i] + " ";
			break;
		}
	}
	response[c_action_acl] = c_success;

	switch (commandType) {
	case srch2::instantsearch::Acl_Record_Add:
		response[c_detail] = " Resource ids added successfully";
		break;
	case srch2::instantsearch::Acl_Record_Append:
		response[c_detail] = " Resource ids appended successfully";
		break;
	case srch2::instantsearch::Acl_Record_Delete:
		response[c_detail] = " Resource ids deleted successfully";
		break;
	default:
		ASSERT(false);
		break;
	};
	return response;
}

