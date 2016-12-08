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

#ifndef __QUERYRESULTS_H__
#define __QUERYRESULTS_H__

#include <vector>
#include <string>
#include <map>

#include <instantsearch/platform.h>
#include <instantsearch/Constants.h>
namespace srch2
{
namespace instantsearch
{

class Query;
class QueryEvaluator;
class TypedValue;
class QueryResultsInternal;
class QueryResultFactoryInternal;



class QueryResultFactory{
public:
	QueryResultFactory();
    ~QueryResultFactory();
    QueryResultFactoryInternal * impl;
};

/**
 * This class defines QueryResults that acts as a container to hold
 * the results of IndexSearcher search methods. A single QueryResults
 * object can be used for paginating through results. For example,
 * consider a search query "John Lennon".  We can create a
 * QueryResults object on the query and retrieve the first K
 * results. The same QueryResults object can be used to retrieve the
 * next K results and so on.
 */
class MYLIB_EXPORT QueryResults
{
public:



    /**
     * TODO: change this to constructor. Also verify how much code changes will be
     * required to this.
     * Creates a QueryResults object.
     * @param[in] indexSearcher the reference to an IndexSearcher object.
     * @param[in] query the reference to a Query object.
     */
    QueryResults(QueryResultFactory * resultsFactory, const QueryEvaluator* queryEvaluator, Query* query);


	QueryResults();
	void init(QueryResultFactory * resultsFactory, const QueryEvaluator* queryEvaluator, Query* query);


    /**
     * Checks if the iterator reaches the end. @return false if
     * there is no next item in the QueryResults.
     */
    /*virtual bool next() = 0;*/

    /**
     * Gets the number of result items in the QueryResults object.
     */
    unsigned getNumberOfResults() const;

    /**
     * Gets the current record id while iterating through the QueryResults object.
     */
    /*virtual int getRecordId() = 0;*/

    /**
     * Gets the record id of the 'position'-th item in the QueryResults object.
     */
    std::string getRecordId(unsigned position) const ;

    /**
     * Gets the record id of the 'position'-th item in the QueryResults object.
     * TODO: Move/remove getInternalRecordId to internal include files. There should be a mapping from external
     * Used to access the InMemoryData.
     */
    unsigned getInternalRecordId(unsigned position) const ;

    /*
     *   this function is called only from unit test. Do not use it in wrapper layer.
     *   The meaning of _Safe is that inside this function we acquire a S lock on
     *   global_readers_writers_mutex and deeper in forward index we get a new copy
     *   of readview. This function is independent in terms of mutex locking and
     *   readview reading; and this is why this method is only used in test and must not be
     *   used anywhere else.
     */
    std::string getInMemoryRecordString_Safe(unsigned position) const ;

    /**
     * Gets the score of the 'position'-th item in the QueryResults object.
     */
    std::string getResultScoreString(unsigned position) const ;
    TypedValue getResultScore(unsigned position) const ;

    /**
     *
     * Gets the matching keywords of the 'position'-th item in
     * the QueryResults object. For each query keyword return a matching
     * keyword in a record. If the query keyword is prefix then return a
     * matching prefix. In the case of multiple matching keywords for a
     * query keyword,the best match based on edit distance is returned.
     *
     * For example, for the query "ulman compilor", a result
     * record with keywords "ullman principles compiler" will
     * return ["ullman","compiler"] in the matchingKeywords
     * vector.  In particular, "compiler" is the best matching
     * keyword in the record for the second query keyword
     * "compilor".
     */
    void getMatchingKeywords(const unsigned position, std::vector<std::string> &matchingKeywords) const ;

    /**
     * Gets the edit distances of the 'position'-th item in the
     * QueryResults object.
     * @param[in] position the position of a result in the returned
     * results (starting from 0).
     * @param[out] editDistances a vector of edit distances of the
     * best matching keywords in this result record with respect
     * to the query keywords.
     *
     * For example, for the query "ulman compilor", a result
     * recorprivate:
    struct Impl;
    Impl *impl;d with keywords "ullman principles conpiler" will
     * return [1,2] in the editDistances vector.  In particular,
     * "2" means that the best matching keyword ("conpiler") in
     * this record has an edit distance 2 to the second query
     * keyword "compilor".
     */
    void getEditDistances(const unsigned position, std::vector<unsigned> &editDistances) const ;

    // The following function only works for attribute based search
    void getMatchedAttributes(const unsigned position, std::vector<std::vector<unsigned> > &matchedAttributes) const ;

    void getTermTypes(unsigned position, std::vector<TermType>& tt) const;
    /*
     *   In Geo search return distance between location of the result and center of the query rank.
     *   TODO: Change the name to getGeoDistance()
     */
    double getPhysicalDistance(const unsigned position) const ;

    const std::map<std::string, std::pair< FacetType , std::vector<std::pair<std::string, float> > > > * getFacetResults() const;

    void copyForPostProcessing(QueryResults * sourceQueryResults) const ;

    void clear();

    bool isResultsApproximated() const;

    // This function copies the estimated number of results if this number is calculated  and returns true (for example in case of topk).
    // If this number is not available, this function returns -1
    long int getEstimatedNumberOfResults() const;


    //TODO: These three functions for internal debugging. remove from the header
    void printStats() const ;

    void printResult() const ;

    void addMessage(const char* msg) ;

    /**
     * Destructor of the QueryResults object.
     */
    /*
     * If a class has a virtual function, then it is possible that some other class extends the base one
     * and in this case the destructor also must be virtual. So if we have a virtual function in a class, the destructor also
     * must be virtual.
     */
    virtual ~QueryResults();





    QueryResultsInternal *impl;
};



}}

#endif /* __QUERYRESULTS_H__ */
