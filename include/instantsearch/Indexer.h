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

#ifndef __INDEXER_H__
#define __INDEXER_H__

#include <instantsearch/platform.h>
#include <instantsearch/Analyzer.h>
#include <instantsearch/Schema.h>
#include <instantsearch/GlobalCache.h>
#include <instantsearch/Record.h>
#include <instantsearch/Constants.h>

#include <string>
#include <stdint.h>

namespace srch2
{
namespace instantsearch
{

class Analyzer;
class Schema;
class Record;
class GlobalCache;
class AttributeAccessControl;
class FeedbackIndex;
class IndexMetaData
{
public:
    IndexMetaData( GlobalCache *_cache,
                   unsigned _mergeEveryNSeconds,
                   unsigned _mergeEveryMWrites,
                   unsigned _updateHistogramEveryPMerges,
                   unsigned _updateHistogramEveryQWrites,
                   const std::string &_directoryName)
    {
        cache = _cache;

        if (_mergeEveryNSeconds == 0)
        {
            _mergeEveryNSeconds = 5;
        }
        mergeEveryNSeconds = _mergeEveryNSeconds;

        if (_mergeEveryMWrites == 0)
        {
            _mergeEveryMWrites = 5;
        }
        mergeEveryMWrites = _mergeEveryMWrites;

        // we are going to update the histogram information every 10 merges.
        if(_updateHistogramEveryPMerges == 0){
        	_updateHistogramEveryPMerges = 10;
        }
        updateHistogramEveryPMerges = _updateHistogramEveryPMerges;

        // we are going to update the histogram information every 50 writes
        if(_updateHistogramEveryQWrites == 0){
        	_updateHistogramEveryQWrites = 50;
        }
        updateHistogramEveryQWrites = _updateHistogramEveryPMerges * _mergeEveryMWrites;


        directoryName = _directoryName;
        maxFeedbackRecordsPerQuery = 1; // set for test cases
        maxCountOfFeedbackQueries = 1; // set for test cases
    }
    
    ~IndexMetaData()
    {
        delete cache;
    }

    std::string directoryName;
    GlobalCache *cache;
    unsigned mergeEveryNSeconds;
    unsigned mergeEveryMWrites;
    unsigned updateHistogramEveryPMerges;
    unsigned updateHistogramEveryQWrites;
    unsigned maxFeedbackRecordsPerQuery;
    unsigned maxCountOfFeedbackQueries;
};




class Indexer
{
public:
    static Indexer* create(IndexMetaData* index, Analyzer *analyzer, Schema *schema);
    static Indexer* load(IndexMetaData* index);

    virtual ~Indexer() {};

    /*virtual const Index *getReadView_NoToken() = 0;*/
    /*virtual GlobalCache *getCache() = 0;*/

    /*
    * Adds a record. If primary key is duplicate, insert fails and -1 is returned. Otherwise, 0 is returned.*/
    virtual INDEXWRITE_RETVAL addRecord(const Record *record, Analyzer *analyzer) = 0;

    // Edits the record's access list based on the command type
    virtual INDEXWRITE_RETVAL aclRecordModifyRoles(const std::string &resourcePrimaryKeyID, vector<string> &roleIds, RecordAclCommandType commandType) = 0;

    // Deletes the role id from the permission map
    // we use this function for deleting a record from a role core
    // then we need to delete this record from the permission map of the resource cores of this core
    virtual INDEXWRITE_RETVAL deleteRoleRecord(const std::string &rolePrimaryKeyID) = 0;

    /*
    * Deletes all the records.*/
    virtual INDEXWRITE_RETVAL deleteRecord(const std::string &primaryKeyID) = 0;

    /*
    * Deletes all the records.
      Get deleted internal record id */
    virtual INDEXWRITE_RETVAL deleteRecordGetInternalId(const std::string &primaryKeyID, unsigned &internalRecordId) = 0;

    /*
      Resume a deleted record */
    virtual INDEXWRITE_RETVAL recoverRecord(const std::string &primaryKeyID, unsigned internalRecordId) = 0;

    /*
      Check if a record exists */
    virtual INDEXLOOKUP_RETVAL lookupRecord(const std::string &primaryKeyID) = 0;

    virtual uint32_t getNumberOfDocumentsInIndex() const = 0;

    virtual FeedbackIndex* getFeedbackIndexer() = 0;

    virtual const std::string getIndexHealth() const = 0;

    virtual const srch2::instantsearch::Schema *getSchema() const = 0;

    // get schema, which can be modified without rebuilding the index
    virtual srch2::instantsearch::Schema *getSchema() = 0;

    virtual StoredRecordBuffer getInMemoryData(unsigned internalRecordId) const = 0;

    virtual const AttributeAccessControl & getAttributeAcl() const = 0;

    /**
     * Builds the index. The records are made searchable after the first commit.
     * It is advised to call the commit in a batch mode. The first commit should be called when
     * the bulk loading of initial records is done. Subsequent commits should be called based on
     * different criteria. For example, we may call "commit()" after a certain number of records
     * have been added to indexes (not yet searchable) or another certain number of seconds have
     * passed since last the commit or when a certain event occurs.
     * Note:- In order to avoid explicit commits after the first commit, we could choose to call
     * startMergeThreadLoop() function.
     */
    virtual INDEXWRITE_RETVAL commit() = 0;
    
    virtual const bool isCommited() const = 0;

    /*
     * Saves the indexes to disk, in the "directoryName" folder.*/

    virtual void save(const std::string& directoryName) = 0;
    virtual void save() = 0;

    virtual void exportData(const string &exportedDataFileName) = 0;

    /*For testing purpose only. Do not use in wrapper code*/
    virtual void merge_ForTesting() = 0;
    /*
    * Deletes all the records.*/
    /*virtual int deleteAll() = 0;*/
    //virtual int merge() = 0;

    /*
     *  Starts a conditional wait loop and merges all the incremental changes to indexes when a
     *  certain condition occurs.
     *  Condition:  n records have been added or t seconds have passed ( whichever occurs first)
     *  Note: This function starts a separate dedicated thread and returns thread id
     */
    virtual pthread_t createAndStartMergeThreadLoop() = 0;
};

}}

#endif //__INDEXER_H__
