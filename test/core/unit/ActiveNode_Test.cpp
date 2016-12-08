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

#include "index/Trie.h"
#include "operation/ActiveNode.h"
#include "util/Assert.h"
#include <iostream>
#include <functional>
#include <vector>
#include <cstring>
#include <assert.h>

namespace srch2is = srch2::instantsearch;
using namespace std;
using srch2is::Trie;
using srch2is::TrieNode;
using srch2is::PrefixActiveNodeSet;
using srch2is::print_trace;
using srch2is::ActiveNodeSetIterator;
using srch2is::LeafNodeSetIteratorForPrefix;

// a structure to record information of each node
class LeafIteratorResultItem
{
public:
    string prefix;
    string keyword;
    unsigned distance;

    LeafIteratorResultItem(const string &prefix, const string &keyword, unsigned distance)
    {
        this->prefix = prefix;
        this->keyword = keyword;
        this->distance = distance;
    }
};

bool checkContainment(vector<LeafIteratorResultItem> &leafIteratorResultVector, const string &prefix, const string &keyword, unsigned distance)
{
    for (unsigned i = 0; i < leafIteratorResultVector.size(); i++)
    {
        if (leafIteratorResultVector.at(i).prefix == prefix &&
                leafIteratorResultVector.at(i).keyword == keyword &&
                leafIteratorResultVector.at(i).distance == distance)
            return true;
    }

    return false;
}


bool checkContainment(vector<std::pair<string, unsigned> > &stringDistanceVector, std::pair<string, unsigned> stringDistance)
{
    for (unsigned i = 0; i < stringDistanceVector.size(); i++)
    {
        if (stringDistanceVector.at(i).first == stringDistance.first &&
                stringDistanceVector.at(i).second == stringDistance.second)
            return true;
    }

    return false;
}


bool checkContainment(vector<string> &prefixVector, const string &prefix)
{
    for (unsigned i = 0; i < prefixVector.size(); i++)
    {
        if (prefixVector.at(i) == prefix)
            return true;
    }

    return false;
}



/*
 * The final trie should look like the following:
 *    root
 *      |
 *      --- c -- a  -- n -- a -- d -- a
 *      |        |     |
 *      d        t     ---- c -- e -- r
 *      |              |
 *      o              ---- t -- e -- e -- n
 *      |
 *      g
 *
 */
Trie *constructTrie()
{
    const string filenameTmp("tmp");
    //Trie *trie = new Trie(filenameTmp, INDEX_BUILD);
    Trie *trie = new Trie();

    unsigned invertedIndexOffset;

    // CHENLI: set the two "end words" so that we do not need to reassign ids to new keywords
    trie->addKeyword(getCharTypeVector("a"), invertedIndexOffset);
    trie->addKeyword(getCharTypeVector("zzzzzzzzzzzzzz"), invertedIndexOffset);

    trie->addKeyword(getCharTypeVector("cancer"), invertedIndexOffset);
    trie->addKeyword(getCharTypeVector("canada"), invertedIndexOffset);
    trie->addKeyword(getCharTypeVector("canteen"), invertedIndexOffset);
    trie->addKeyword(getCharTypeVector("can"), invertedIndexOffset);
    trie->addKeyword(getCharTypeVector("cat"), invertedIndexOffset);
    trie->addKeyword(getCharTypeVector("dog"), invertedIndexOffset);
    trie->commit();
    trie->finalCommit_finalizeHistogramInformation(NULL, NULL , 0);

    return trie;
}

void printPrefixes(vector<string> &similarPrefixes)
{
    for (unsigned i = 0; i < similarPrefixes.size(); i++)
        cout << similarPrefixes.at(i) << "\n";
}


void testPrefixActiveNodeSet()
{
    Trie *trie = constructTrie();

    boost::shared_ptr<PrefixActiveNodeSet> prefixActiveNodeSet;
    boost::shared_ptr<PrefixActiveNodeSet> newPrefixActiveNodeSet;
    vector<string> similarPrefixes;

    // case 1: an empty string of exact matching
    prefixActiveNodeSet.reset(new PrefixActiveNodeSet(trie, 0));
    ASSERT(prefixActiveNodeSet->getPrefixUtf8String() == "");

    prefixActiveNodeSet->getComputedSimilarPrefixes(trie, similarPrefixes);
    ASSERT(similarPrefixes.size() == 1); // only one active node    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('c');

    ASSERT(checkContainment(similarPrefixes, ""));
    similarPrefixes.clear();

    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('c');
    newPrefixActiveNodeSet->getComputedSimilarPrefixes(trie, similarPrefixes);
    ASSERT(similarPrefixes.size() == 1); // still only one active node
    ASSERT(checkContainment(similarPrefixes, "c"));
    similarPrefixes.clear();

    // case 2: an empty string with an edit distance 1
    prefixActiveNodeSet.reset(new PrefixActiveNodeSet(trie, 1));
    prefixActiveNodeSet->getComputedSimilarPrefixes(trie, similarPrefixes);

    ASSERT(similarPrefixes.size() == 1);
    ASSERT(checkContainment(similarPrefixes, ""));
    //ASSERT(checkContainment(similarPrefixes, "c"));
    //ASSERT(checkContainment(similarPrefixes, "d"));
    similarPrefixes.clear();

    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('c');
    // newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('c');
    //ASSERT(*(newPrefixActiveNodeSet->getPrefix()) == "c");

    newPrefixActiveNodeSet->getComputedSimilarPrefixes(trie, similarPrefixes);
    ASSERT(newPrefixActiveNodeSet->getNumberOfActiveNodes() == 2);
    ASSERT(checkContainment(similarPrefixes, ""));
    ASSERT(checkContainment(similarPrefixes, "c"));
    //ASSERT(checkContainment(similarPrefixes, "d"));
    //ASSERT(checkContainment(similarPrefixes, "ca"));
    similarPrefixes.clear();



    // case 3: an empty string with an edit distance 2
    prefixActiveNodeSet.reset(new PrefixActiveNodeSet(trie, 2));
    prefixActiveNodeSet->getComputedSimilarPrefixes(trie, similarPrefixes);

    ASSERT(similarPrefixes.size() == 1);
    ASSERT(checkContainment(similarPrefixes, ""));
    //ASSERT(checkContainment(similarPrefixes, "c"));
    //ASSERT(checkContainment(similarPrefixes, "ca"));
    //ASSERT(checkContainment(similarPrefixes, "d"));
    //ASSERT(checkContainment(similarPrefixes, "do"));
    similarPrefixes.clear();

    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('c');
    //ASSERT(*(newPrefixActiveNodeSet->getPrefix()) == "c");

    newPrefixActiveNodeSet->getComputedSimilarPrefixes(trie, similarPrefixes);
    ASSERT(similarPrefixes.size() == 2);
    ASSERT(checkContainment(similarPrefixes, ""));
    ASSERT(checkContainment(similarPrefixes, "c"));
    //ASSERT(checkContainment(similarPrefixes, "ca"));
    //ASSERT(checkContainment(similarPrefixes, "can"));
    //ASSERT(checkContainment(similarPrefixes, "cat"));
    //ASSERT(checkContainment(similarPrefixes, "d"));
    //ASSERT(checkContainment(similarPrefixes, "do"));
    similarPrefixes.clear();


    // case 4: an empty string with an edit distance 3
    prefixActiveNodeSet.reset(new PrefixActiveNodeSet(trie, 3));
    prefixActiveNodeSet->getComputedSimilarPrefixes(trie, similarPrefixes);
    ASSERT(similarPrefixes.size() == 1);
    ASSERT(checkContainment(similarPrefixes, ""));
    //ASSERT(checkContainment(similarPrefixes, "c"));
    //ASSERT(checkContainment(similarPrefixes, "ca"));
    //ASSERT(checkContainment(similarPrefixes, "can"));
    //ASSERT(checkContainment(similarPrefixes, "cat"));
    //ASSERT(checkContainment(similarPrefixes, "d"));
    //ASSERT(checkContainment(similarPrefixes, "do"));
    //ASSERT(checkContainment(similarPrefixes, "dog"));
    similarPrefixes.clear();

    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('c');
    newPrefixActiveNodeSet->getComputedSimilarPrefixes(trie, similarPrefixes);
    ASSERT(similarPrefixes.size() == 3);
    ASSERT(checkContainment(similarPrefixes, ""));
    ASSERT(checkContainment(similarPrefixes, "c"));
    //ASSERT(checkContainment(similarPrefixes, "ca"));
    //ASSERT(checkContainment(similarPrefixes, "can"));
    //ASSERT(checkContainment(similarPrefixes, "cat"));
    //ASSERT(checkContainment(similarPrefixes, "cana"));
    ASSERT(checkContainment(similarPrefixes, "canc"));
    //ASSERT(checkContainment(similarPrefixes, "cant"));
    //ASSERT(checkContainment(similarPrefixes, "d"));
    //ASSERT(checkContainment(similarPrefixes, "do"));
    //ASSERT(checkContainment(similarPrefixes, "dog"));
    similarPrefixes.clear();


    // case 5.1: try a longer string. start with an empty string with an edit distance 1 and deletion
    prefixActiveNodeSet.reset(new PrefixActiveNodeSet(trie, 1));
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('c'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('a'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('c'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('e');
    //ASSERT(*(newPrefixActiveNodeSet->getPrefix()) == "cace");


    newPrefixActiveNodeSet->getComputedSimilarPrefixes(trie, similarPrefixes);
    ASSERT(similarPrefixes.size() == 1);
    ASSERT(checkContainment(similarPrefixes, "cance"));
    similarPrefixes.clear();


    // case 5.2: try a longer string. start with an empty string with an edit distance 1 and substitution
    prefixActiveNodeSet.reset(new PrefixActiveNodeSet(trie, 1));
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('c'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('e'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('n'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('c');
    //ASSERT(*(newPrefixActiveNodeSet->getPrefix()) == "cace");


    newPrefixActiveNodeSet->getComputedSimilarPrefixes(trie, similarPrefixes);
    ASSERT(similarPrefixes.size() == 1);
    ASSERT(checkContainment(similarPrefixes, "canc"));
    similarPrefixes.clear();

    // case 5.3: try a longer string. start with an empty string with an edit distance 1 and insertion
    prefixActiveNodeSet.reset(new PrefixActiveNodeSet(trie, 1));
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('c'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('a'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('n'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('n'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('c');
    //ASSERT(*(newPrefixActiveNodeSet->getPrefix()) == "cace");


    newPrefixActiveNodeSet->getComputedSimilarPrefixes(trie, similarPrefixes);
    ASSERT(similarPrefixes.size() == 1);
    ASSERT(checkContainment(similarPrefixes, "canc"));
    similarPrefixes.clear();

    // case 5.4: try a longer string. start with an empty string with an edit distance 1 and matching correctly
    prefixActiveNodeSet.reset(new PrefixActiveNodeSet(trie, 1));
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('c'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('a'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('n'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('c');
    //ASSERT(*(newPrefixActiveNodeSet->getPrefix()) == "cace");


    newPrefixActiveNodeSet->getComputedSimilarPrefixes(trie, similarPrefixes);
    ASSERT(similarPrefixes.size() == 2);
    ASSERT(checkContainment(similarPrefixes, "canc"));
    ASSERT(checkContainment(similarPrefixes, "can"));
    similarPrefixes.clear();

    // case 6.1: try a longer string. start with an empty string with an edit distance 2 and deletion
    prefixActiveNodeSet.reset(new PrefixActiveNodeSet(trie, 2));
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('n'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('c'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('e');
    //ASSERT(*(newPrefixActiveNodeSet->getPrefix()) == "nce");

    newPrefixActiveNodeSet->getComputedSimilarPrefixes(trie, similarPrefixes);
    ASSERT(similarPrefixes.size() == 2);
    ASSERT(checkContainment(similarPrefixes, "c"));
    //ASSERT(checkContainment(similarPrefixes, "ca"));
    ASSERT(checkContainment(similarPrefixes, "cance"));
    similarPrefixes.clear();

    // case 6.2: try a longer string. start with an empty string with an edit distance 2 and substitution
    prefixActiveNodeSet.reset(new PrefixActiveNodeSet(trie, 2));
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('k'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('a'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('n'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('k'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('e');
    //ASSERT(*(newPrefixActiveNodeSet->getPrefix()) == "nce");

    newPrefixActiveNodeSet->getComputedSimilarPrefixes(trie, similarPrefixes);
    ASSERT(similarPrefixes.size() == 2);
    //ASSERT(checkContainment(similarPrefixes, "c"));
    //ASSERT(checkContainment(similarPrefixes, "ca"));
    ASSERT(checkContainment(similarPrefixes, "cance"));
    ASSERT(checkContainment(similarPrefixes, "cante"));
    similarPrefixes.clear();

    // case 6.3: try a longer string. start with an empty string with an edit distance 2 and insertion
    prefixActiveNodeSet.reset(new PrefixActiveNodeSet(trie, 2));
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('c'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('a'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('n'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('n'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('c'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('c'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('e');
    //ASSERT(*(newPrefixActiveNodeSet->getPrefix()) == "nce");

    newPrefixActiveNodeSet->getComputedSimilarPrefixes(trie, similarPrefixes);
    ASSERT(similarPrefixes.size() == 1);
    //ASSERT(checkContainment(similarPrefixes, "c"));
    //ASSERT(checkContainment(similarPrefixes, "ca"));
    ASSERT(checkContainment(similarPrefixes, "cance"));
    similarPrefixes.clear();

    // case 6.4: try a longer string. start with an empty string with an edit distance 2 and exactly matching
    prefixActiveNodeSet.reset(new PrefixActiveNodeSet(trie, 2));
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('c'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('a'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('n'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('c'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('e');
    //ASSERT(*(newPrefixActiveNodeSet->getPrefix()) == "nce");

    newPrefixActiveNodeSet->getComputedSimilarPrefixes(trie, similarPrefixes);
    ASSERT(similarPrefixes.size() == 5);
    ASSERT(checkContainment(similarPrefixes, "can"));
    ASSERT(checkContainment(similarPrefixes, "canc"));
    ASSERT(checkContainment(similarPrefixes, "cance"));
    ASSERT(checkContainment(similarPrefixes, "cante"));
    ASSERT(checkContainment(similarPrefixes, "cantee"));
    similarPrefixes.clear();

    // finally, we can delete the trie
    delete trie;
    /**/
}


Trie *constructNoFuzzyTrie()
{
    const string filenameTmp("tmp");
    //Trie *trie = new Trie(filenameTmp, INDEX_BUILD);
    Trie *trie = new Trie();

    unsigned invertedIndexOffset;
    trie->addKeyword(getCharTypeVector("ㄉㄨㄥ"), invertedIndexOffset);
    trie->addKeyword(getCharTypeVector("ㄎㄨㄥ"), invertedIndexOffset);
    trie->addKeyword(getCharTypeVector("ㄕㄢ"), invertedIndexOffset);

    trie->commit();
    trie->finalCommit_finalizeHistogramInformation(NULL , NULL, 0);

    return trie;
}

void testNoFuzzyPrefixActiveNodeSet()
{

        Trie *trie = constructNoFuzzyTrie();

        boost::shared_ptr<PrefixActiveNodeSet> prefixActiveNodeSet;
        boost::shared_ptr<PrefixActiveNodeSet> newPrefixActiveNodeSet;
        vector<string> similarPrefixes;

        prefixActiveNodeSet.reset(new PrefixActiveNodeSet(trie, 2));
        vector<CharType> charTypeVector = getCharTypeVector("ㄉㄨㄥ");
        for(unsigned i = 0; i < charTypeVector.size(); i++){
			newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally(charTypeVector[i]);
			prefixActiveNodeSet = newPrefixActiveNodeSet;

        }

        prefixActiveNodeSet->getComputedSimilarPrefixes(trie, similarPrefixes);
        ASSERT(similarPrefixes.size() == 1);
        ASSERT(checkContainment(similarPrefixes, "ㄉㄨㄥ"));

        // finally, we can delete the trie
        delete trie;
}

void testPrefixIterators()
{
    Trie *trie = constructTrie();
    vector<std::pair<string, unsigned> > stringDistanceVector;
    boost::shared_ptr<PrefixActiveNodeSet> prefixActiveNodeSet;
    boost::shared_ptr<PrefixActiveNodeSet> newPrefixActiveNodeSet;

    prefixActiveNodeSet.reset(new PrefixActiveNodeSet(trie, 2));

    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('n'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('c'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('e');
    ASSERT(newPrefixActiveNodeSet->getPrefixUtf8String() == "nce");

    stringDistanceVector.clear();
    // test ActiveNodeSetIterator (with the edit distance bound "2")
    for (ActiveNodeSetIterator iter(newPrefixActiveNodeSet.get(), 2); !iter.isDone(); iter.next())
    {
        const TrieNode *trieNode;
        unsigned distance;
        iter.getItem(trieNode, distance);
        std::vector<CharType> prefix;
        trie->getPrefixString_NotThreadSafe(trieNode, prefix);
        string prefixStr;
        charTypeVectorToUtf8String(prefix, prefixStr);
        stringDistanceVector.push_back(std::make_pair(prefixStr, distance));
    }

    ASSERT(stringDistanceVector.size() == 2);
    ASSERT(checkContainment(stringDistanceVector, std::make_pair("c", 2)));
    //ASSERT(checkContainment(stringDistanceVector, std::make_pair("ca", 2)));
    ASSERT(checkContainment(stringDistanceVector, std::make_pair("cance", 2)));
    stringDistanceVector.clear();

    // test LeafNodeSetIteratorForPrefix
    vector<LeafIteratorResultItem> leafIteratorResultVector;
    for (LeafNodeSetIteratorForPrefix lnsi(newPrefixActiveNodeSet.get(), 2); !lnsi.isDone(); lnsi.next()) {
        const TrieNode *prefixNode;
        const TrieNode *leafNode;
        unsigned distance;
        lnsi.getItem(prefixNode, leafNode, distance);

        ASSERT(leafNode->isTerminalNode());
        vector<CharType> prefix;
        trie->getPrefixString_NotThreadSafe(prefixNode, prefix);
        vector<CharType> keyword;
        string prefixStr;
        string keywordStr;
        charTypeVectorToUtf8String(prefix, prefixStr);
        charTypeVectorToUtf8String(keyword, keywordStr);
        trie->getPrefixString_NotThreadSafe(leafNode, keyword);
        leafIteratorResultVector.push_back(LeafIteratorResultItem(prefixStr, keywordStr, distance));
    }
    ASSERT(leafIteratorResultVector.size() == 4);

    //LeafIteratorResultItem liri("c", "cat", 2); ASSERT(checkContainment(leafIteratorResultVector, liri));
    ASSERT(checkContainment(leafIteratorResultVector, "c", "can", 2));
    ASSERT(checkContainment(leafIteratorResultVector, "c", "can", 2));
    ASSERT(checkContainment(leafIteratorResultVector, "c", "canada", 2));
    // TODO: prefix might not be unique. decide if we should return the longest matching prefix
    // ASSERT(checkContainment(leafIteratorResultVector, "c", "cancer", 2));
    ASSERT(checkContainment(leafIteratorResultVector, "c", "canteen", 2));
    leafIteratorResultVector.clear();

    delete trie;
}

// test those iterators with an edit distance range
void testPrefixIteratorsWithRanges()
{
    Trie *trie = constructTrie();

    vector<std::pair<string, unsigned> > stringDistanceVector;
    boost::shared_ptr<PrefixActiveNodeSet> prefixActiveNodeSet;
    boost::shared_ptr<PrefixActiveNodeSet> newPrefixActiveNodeSet;

    prefixActiveNodeSet.reset(new PrefixActiveNodeSet(trie, 2));


    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('c'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('a'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('n');
    ASSERT(newPrefixActiveNodeSet->getPrefixUtf8String() == "can");

    stringDistanceVector.clear();
    // test ActiveNodeSetIterator (with the edit distance bound "1")
    for (ActiveNodeSetIterator iter(newPrefixActiveNodeSet.get(), 1); !iter.isDone(); iter.next())
    {
        const TrieNode *trieNode;
        unsigned distance;
        iter.getItem(trieNode, distance);
        vector<CharType> prefix;
        trie->getPrefixString_NotThreadSafe(trieNode, prefix);
        string prefixStr;
        charTypeVectorToUtf8String(prefix, prefixStr);
        stringDistanceVector.push_back(std::make_pair(prefixStr, distance));
    }

    ASSERT(stringDistanceVector.size() == 2);
    ASSERT(checkContainment(stringDistanceVector, std::make_pair("ca", 1)));
    //ASSERT(checkContainment(stringDistanceVector, std::make_pair("cat", 1)));
    ASSERT(checkContainment(stringDistanceVector, std::make_pair("can", 0)));
    //ASSERT(checkContainment(stringDistanceVector, std::make_pair("cana", 1)));
    //ASSERT(checkContainment(stringDistanceVector, std::make_pair("canc", 1)));
    //ASSERT(checkContainment(stringDistanceVector, std::make_pair("cant", 1)));
    stringDistanceVector.clear();

    vector<LeafIteratorResultItem> leafIteratorResultVector;
    // test LeafNodeSetIterator (using "1" as an upper bound of edit distance)
    for (LeafNodeSetIteratorForPrefix lnsi(newPrefixActiveNodeSet.get(), 1); !lnsi.isDone(); lnsi.next()) {
        const TrieNode *prefixNode;
        const TrieNode *leafNode;
        unsigned distance;
        lnsi.getItem(prefixNode, leafNode, distance);

        ASSERT(leafNode->isTerminalNode());
        vector<CharType>  prefix;
        trie->getPrefixString_NotThreadSafe(prefixNode, prefix);
        vector<CharType>  keyword;
        trie->getPrefixString_NotThreadSafe(leafNode, keyword);
        string prefixStr;
        string keywordStr;
        charTypeVectorToUtf8String(prefix, prefixStr);
        charTypeVectorToUtf8String(keyword, keywordStr);
        leafIteratorResultVector.push_back(LeafIteratorResultItem(prefixStr, keywordStr, distance));
    }

    ASSERT(leafIteratorResultVector.size() == 4);
    ASSERT(checkContainment(leafIteratorResultVector, "can", "can", 0));
    ASSERT(checkContainment(leafIteratorResultVector, "can", "canada", 0));
    ASSERT(checkContainment(leafIteratorResultVector, "can", "cancer", 0));
    ASSERT(checkContainment(leafIteratorResultVector, "can", "canteen", 0));
    // TODO: prefix might not be unique. decide if we should return the longest matching prefix
    //ASSERT(checkContainment(leafIteratorResultVector, LeafIteratorResultItem("ca", "cat", 1)));
    leafIteratorResultVector.clear();

    // test LeafNodeSetIterator (using "0" as an upper bound of edit distance)
    for (LeafNodeSetIteratorForPrefix lnsi(newPrefixActiveNodeSet.get(), 0); !lnsi.isDone(); lnsi.next()) {
        const TrieNode *prefixNode;
        const TrieNode *leafNode;
        unsigned distance;
        lnsi.getItem(prefixNode, leafNode, distance);

        ASSERT(leafNode->isTerminalNode());
        vector<CharType>  prefix;
        trie->getPrefixString_NotThreadSafe(prefixNode, prefix);
        vector<CharType>  keyword;
        trie->getPrefixString_NotThreadSafe(leafNode, keyword);
        /*string prefix = trie->getPrefixString(prefixNode);
        string keyword = trie->getPrefixString(leafNode);*/
        string prefixStr;
        string keywordStr;
        charTypeVectorToUtf8String(prefix, prefixStr);
        charTypeVectorToUtf8String(keyword, keywordStr);
        leafIteratorResultVector.push_back(LeafIteratorResultItem(prefixStr, keywordStr, distance));
    }

    ASSERT(leafIteratorResultVector.size() == 4);
    ASSERT(checkContainment(leafIteratorResultVector, "can", "can", 0));
    ASSERT(checkContainment(leafIteratorResultVector, "can", "canada", 0));
    ASSERT(checkContainment(leafIteratorResultVector, "can", "cancer", 0));
    ASSERT(checkContainment(leafIteratorResultVector, "can", "canteen", 0));
    leafIteratorResultVector.clear();

    delete trie;
}

void testCompleteIterators()
{
    Trie *trie = constructTrie();

    vector<std::pair<string, unsigned> > leafIteratorResultVector;
    boost::shared_ptr<PrefixActiveNodeSet> prefixActiveNodeSet;
    boost::shared_ptr<PrefixActiveNodeSet> newPrefixActiveNodeSet;

    prefixActiveNodeSet.reset(new PrefixActiveNodeSet(trie, 0));

    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('c'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('a'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('n');
    ASSERT(newPrefixActiveNodeSet->getPrefixUtf8String() == "can");

    leafIteratorResultVector.clear();
    // test ActiveNodeSetIterator
    for (ActiveNodeSetIterator iter(newPrefixActiveNodeSet.get(), 0); !iter.isDone(); iter.next())
    {
        const TrieNode *trieNode;
        unsigned distance;
        iter.getItem(trieNode, distance);

        if(trieNode->isTerminalNode())
        {
            vector<CharType> prefix;
            trie->getPrefixString_NotThreadSafe(trieNode, prefix);
            string prefixStr;
            charTypeVectorToUtf8String(prefix, prefixStr);
            leafIteratorResultVector.push_back(std::make_pair(prefixStr, distance));
        }
    }

    ASSERT(leafIteratorResultVector.size() == 1);
    ASSERT(checkContainment(leafIteratorResultVector, std::make_pair("can", 0)));
    leafIteratorResultVector.clear();

    delete trie;
}

void testActiveNodeWithTrieUpdate()
{
    Trie *trie = constructTrie();

    //unsigned id =
    unsigned invertedIndexOffset;
    //trie->addKeyword_ThreadSafe("aaaaaa", invertedIndexOffset);
    //trie->addKeyword_ThreadSafe("zzzzzz", invertedIndexOffset);
    trie->addKeyword_ThreadSafe(getCharTypeVector("peo"), invertedIndexOffset);
    trie->addKeyword_ThreadSafe(getCharTypeVector("people"), invertedIndexOffset);

    trie->merge(NULL , NULL, 0, false);

    vector<std::pair<string, unsigned> > leafIteratorResultVector;
    boost::shared_ptr<PrefixActiveNodeSet> prefixActiveNodeSet;
    boost::shared_ptr<PrefixActiveNodeSet> newPrefixActiveNodeSet;

    prefixActiveNodeSet.reset(new PrefixActiveNodeSet(trie, 0));

    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('p'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('e'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('o');
    ASSERT(newPrefixActiveNodeSet->getPrefixUtf8String() == "peo");

    leafIteratorResultVector.clear();
    // test ActiveNodeSetIterator
    for (ActiveNodeSetIterator iter(newPrefixActiveNodeSet.get(), 0); !iter.isDone(); iter.next())
    {
        const TrieNode *trieNode;
        unsigned distance;
        iter.getItem(trieNode, distance);
        vector<CharType> word;
        trie->getPrefixString_NotThreadSafe(trieNode, word);
        std::cout << "word:" << word << std::endl;
        if(trieNode->isTerminalNode())
        {
                vector<CharType> prefix;
            trie->getPrefixString_NotThreadSafe(trieNode, prefix);
            string prefixStr;
            charTypeVectorToUtf8String(prefix, prefixStr);
            leafIteratorResultVector.push_back(std::make_pair(prefixStr, distance));
        }
    }

    ASSERT(leafIteratorResultVector.size() == 1);
    ASSERT(checkContainment(leafIteratorResultVector, std::make_pair("peo", 0)));
    leafIteratorResultVector.clear();

    delete trie;
}

void testLeafNodeIteratorWithTrieUpdate()
{
    Trie *trie = constructTrie();

    unsigned invertedIndexOffset;
    //trie->addKeyword_ThreadSafe("aaaaaa", invertedIndexOffset);
    //trie->addKeyword_ThreadSafe("zzzzzz", invertedIndexOffset);
    trie->addKeyword_ThreadSafe(getCharTypeVector("peo"), invertedIndexOffset);
    trie->addKeyword_ThreadSafe(getCharTypeVector("people"), invertedIndexOffset);
    trie->merge(NULL , NULL,  0, false);

    vector<std::pair<string, unsigned> > stringDistanceVector;
    boost::shared_ptr<PrefixActiveNodeSet> prefixActiveNodeSet;
    boost::shared_ptr<PrefixActiveNodeSet> newPrefixActiveNodeSet;

    prefixActiveNodeSet.reset(new PrefixActiveNodeSet(trie, 2));

    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('c'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('a'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('n');
    ASSERT(newPrefixActiveNodeSet->getPrefixUtf8String() == "can");

    vector<LeafIteratorResultItem> leafIteratorResultVector;
    // test LeafNodeSetIterator (using "1" as an upper bound of edit distance)
    for (LeafNodeSetIteratorForPrefix lnsi(newPrefixActiveNodeSet.get(), 1); !lnsi.isDone(); lnsi.next()) {
        const TrieNode *prefixNode;
        const TrieNode *leafNode;
        unsigned distance;
        lnsi.getItem(prefixNode, leafNode, distance);

        ASSERT(leafNode->isTerminalNode());
        vector<CharType> prefix;
        trie->getPrefixString_NotThreadSafe(prefixNode, prefix);
        vector<CharType> keyword;
        trie->getPrefixString_NotThreadSafe(leafNode, keyword);
        string prefixStr;
        string keywordStr;
        charTypeVectorToUtf8String(prefix, prefixStr);
        charTypeVectorToUtf8String(keyword, keywordStr);
        leafIteratorResultVector.push_back(LeafIteratorResultItem(prefixStr, keywordStr, distance));
    }

    ASSERT(leafIteratorResultVector.size() == 5);
    ASSERT(checkContainment(leafIteratorResultVector, "can", "can", 0));
    ASSERT(checkContainment(leafIteratorResultVector, "can", "canada", 0));
    ASSERT(checkContainment(leafIteratorResultVector, "can", "cancer", 0));
    ASSERT(checkContainment(leafIteratorResultVector, "can", "canteen", 0));

    prefixActiveNodeSet.reset(new PrefixActiveNodeSet(trie, 2));
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('p'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('e'); prefixActiveNodeSet = newPrefixActiveNodeSet;
    newPrefixActiveNodeSet = prefixActiveNodeSet->computeActiveNodeSetIncrementally('o');
    ASSERT(newPrefixActiveNodeSet->getPrefixUtf8String() == "peo");

    leafIteratorResultVector.clear();
    // test LeafNodeSetIterator (using "1" as an upper bound of edit distance)
    for (LeafNodeSetIteratorForPrefix lnsi(newPrefixActiveNodeSet.get(), 1); !lnsi.isDone(); lnsi.next()) {
        const TrieNode *prefixNode;
        const TrieNode *leafNode;
        unsigned distance;
        lnsi.getItem(prefixNode, leafNode, distance);

        ASSERT(leafNode->isTerminalNode());
        vector<CharType> prefix;
        trie->getPrefixString_NotThreadSafe(prefixNode, prefix);
        vector<CharType> keyword;
        trie->getPrefixString_NotThreadSafe(leafNode, keyword);
        string prefixStr;
        string keywordStr;
        charTypeVectorToUtf8String(prefix, prefixStr);
        charTypeVectorToUtf8String(keyword, keywordStr);
        leafIteratorResultVector.push_back(LeafIteratorResultItem(prefixStr, keywordStr, distance));
    }

    ASSERT(leafIteratorResultVector.size() == 2);

    newPrefixActiveNodeSet->printActiveNodes(trie);
    LeafNodeSetIteratorForPrefix iterPrint(newPrefixActiveNodeSet.get(), 1);
    iterPrint.printLeafNodes(trie);

    ASSERT(checkContainment(leafIteratorResultVector, "peo", "peo", 0));
    ASSERT(checkContainment(leafIteratorResultVector, "peo", "people", 0));

    delete trie;
}


int main(int argc, char *argv[]) {
    bool verbose = false;
    if (argc > 1 && strcmp(argv[1], "--verbose") == 0) {
        verbose = true;
    }

    testPrefixActiveNodeSet();

    //testNofuzzyPrefixActiveNodeSet();

    //testPrefixIterators();
    //testPrefixIteratorsWithRanges();
    testCompleteIterators();

    testActiveNodeWithTrieUpdate();
    testLeafNodeIteratorWithTrieUpdate();

    cout << "ActiveNodes unit tests: Passed" << endl;

    return 0;
}
