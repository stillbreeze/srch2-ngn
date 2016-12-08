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

#include <instantsearch/Analyzer.h>
#include <instantsearch/Indexer.h>
#include <instantsearch/Query.h>
#include <instantsearch/Term.h>
#include <instantsearch/QueryResults.h>
#include "IntegrationTestHelper.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <cstdlib>

using namespace std;
namespace srch2is = srch2::instantsearch;
using namespace srch2is;

// variables to measure the elapsed time
struct timespec tstart;
struct timespec tend;

struct timespec tstart_each;
struct timespec tend_each;

// variables to measure the elapsed time
struct timespec tstart_d;
struct timespec tend_d;

struct timespec tstart_d_each;
struct timespec tend_d_each;

bool parseLine(string &line, string &query, bool &returnValue1, bool &returnValue2)
{
    vector<string> record;
    csvline_populate(record, line, ',');

    if (record.size() < 3)
      return false;

    query = record[0];
    returnValue1 = atoi(record[1].c_str());
    returnValue2 = atoi(record[2].c_str());

    return true;
}

int main(int argc, char **argv)
{
    std::string index_dir = getenv("index_dir");

    // user defined query variables
    std::vector<std::string> file;
    std::vector<std::string> output_print;
    std::string line;
    file.clear();

    std::string query_file = getenv("query_file");
    std::ifstream infile(query_file.c_str(), std::ios_base::in);
    while (getline(infile, line, '\n')) {
        string query ="";
        bool returnValue1 = 0;
        bool returnValue2 = 0;
        bool pass = parseLine(line, query, returnValue1,returnValue2);
        if (!pass)
            abort();
        file.push_back (query);
    }

    clock_gettime(CLOCK_REALTIME, &tstart);

    {
        // create an index searcher
        //GlobalCache *cache = GlobalCache::create(100000,1000); // To test aCache
        CacheManager *cache = new CacheManager();// create an index writer
        unsigned mergeEveryNSeconds = 3;
        unsigned mergeEveryMWrites = 5;
        unsigned updateHistogramEveryPMerges = 1;
        unsigned updateHistogramEveryQWrites = 5;
        IndexMetaData *indexMetaData = new IndexMetaData(cache,
        		mergeEveryNSeconds, mergeEveryMWrites, updateHistogramEveryPMerges, updateHistogramEveryQWrites, index_dir);
        Indexer *indexer = Indexer::load(indexMetaData);
        QueryEvaluatorRuntimeParametersContainer runtimeParameters;
        QueryEvaluator * queryEvaluator = new QueryEvaluator(indexer, &runtimeParameters);
        const Analyzer *analyzer = getAnalyzer();

        for (vector<string>::iterator vectIter = file.begin(); vectIter!= file.end(); vectIter ++) {
            //for( vector<string>::iterator vectIter = file.begin(); vectIter!= file.begin()+200; vectIter++ )
                //std::cout << *vectIter <<  std::endl;
            clock_gettime(CLOCK_REALTIME, &tstart_each);

            unsigned resultCount = 10;
            pingNormalQuery(analyzer, queryEvaluator,*vectIter,resultCount, vector<unsigned>(), ATTRIBUTES_OP_AND);

            clock_gettime(CLOCK_REALTIME, &tend_each);
            double ts2 = (tend_each.tv_sec - tstart_each.tv_sec) * 1000 + (tend_each.tv_nsec - tstart_each.tv_nsec)/1000000;

            std::ostringstream out;
            out << *vectIter << "," << ts2;
            output_print.push_back(out.str());
        }
        delete queryEvaluator;
        delete analyzer;
        delete indexer;
        delete cache;
    }

    clock_gettime(CLOCK_REALTIME, &tend);
    double ts2 = (tend.tv_sec - tstart.tv_sec) * 1000 + (tend.tv_nsec - tstart.tv_nsec) / 1000000;

    cout << "Double Searcher Ping..." << endl;

    clock_gettime(CLOCK_REALTIME, &tstart_d);
    {
        // create an index searcher
        //GlobalCache *cache = GlobalCache::create(100000,1000); // To test aCache
        CacheManager *cache = new CacheManager();// create an index writer
        unsigned mergeEveryNSeconds = 3;
        unsigned mergeEveryMWrites = 5;
        unsigned updateHistogramEveryPMerges = 1;
        unsigned updateHistogramEveryQWrites = 5;
        IndexMetaData *indexMetaData = new IndexMetaData( cache,
        		mergeEveryNSeconds, mergeEveryMWrites, updateHistogramEveryPMerges, updateHistogramEveryQWrites, index_dir);
        Indexer *indexer = Indexer::load(indexMetaData);
        QueryEvaluatorRuntimeParametersContainer runtimeParameters;
        QueryEvaluator * queryEvaluator = new QueryEvaluator(indexer, &runtimeParameters);
        const Analyzer *analyzer = getAnalyzer();
        for (vector<string>::iterator vectIter = file.begin(); vectIter!= file.end(); vectIter ++) {
            //for( vector<string>::iterator vectIter = file.begin(); vectIter!= file.begin()+200; vectIter++ )
            clock_gettime(CLOCK_REALTIME, &tstart_d_each);

            unsigned resultCount = 10;
            std::cout << "[[" << *vectIter << "]]" << std::endl;
            doubleSearcherPing(analyzer, queryEvaluator,*vectIter,resultCount, 0, vector<unsigned>(), ATTRIBUTES_OP_AND);
    
            clock_gettime(CLOCK_REALTIME, &tend_d_each);
            double ts2 = (tend_d_each.tv_sec - tstart_d_each.tv_sec) * 1000 
                       + (tend_d_each.tv_nsec - tstart_d_each.tv_nsec) / 1000000;

            std::ostringstream out;
            out << *vectIter << "," << ts2;
            output_print.push_back(out.str());
        }
        delete queryEvaluator;
        delete indexer;
        delete cache;
        delete analyzer;
    }

    cout << "Double Searcher Ping done." << endl;

    clock_gettime(CLOCK_REALTIME, &tend_d);
    double ts2_d = (tend_d.tv_sec - tstart_d.tv_sec) * 1000 + (tend_d.tv_nsec - tstart_d.tv_nsec) / 1000000;

    //delete index;
    cout << "Writing to file..." << endl;
    //ofstream outfile("/home/chrysler/DoubleQueryTest_output.txt", ios::out | ios::binary);
    //ofstream outfile(index_dir+"/DoubleQueryTest_output.txt", ios::out | ios::binary);
    ofstream outfile("DoubleQueryTest_output.txt", ios::out | ios::binary);
    for (std::vector<string>::iterator iter = output_print.begin(); iter != output_print.end(); ++iter)
        outfile << *iter << endl;

    cout << "Executed " << file.size() << " queries in " << ts2 << " milliseconds." << endl;
    cout << "Executed " << file.size() << " double_queries in " << ts2_d << " milliseconds." << endl;

    cout << "DoubleQueryStress_Test passed!" << endl;
    
    return 0;
}
