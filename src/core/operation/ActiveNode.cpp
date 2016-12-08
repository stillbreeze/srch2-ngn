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
#include "ActiveNode.h"
#include "util/Logger.h"
using srch2::util::Logger;

namespace srch2
{
namespace instantsearch
{

void PrefixActiveNodeSet::getComputedSimilarPrefixes(const Trie *trie, std::vector<std::string> &similarPrefixes)
{
    for (std::map<const TrieNode*, PivotalActiveNode >::iterator mapIterator = PANMap.begin();
            mapIterator != PANMap.end(); mapIterator ++) {
        const TrieNode *trieNode = mapIterator->first;
        std::string prefix;
        trie->getPrefixString_NotThreadSafe(trieNode, prefix);
        similarPrefixes.push_back(prefix);
    }
}

boost::shared_ptr<PrefixActiveNodeSet> PrefixActiveNodeSet::computeActiveNodeSetIncrementally(const CharType additionalChar)
{
    // form the new string. // TODO (OPT): avoid string copy
    std::vector<CharType> newString = this->prefix;
    newString.push_back(additionalChar);

    PrefixActiveNodeSet *newActiveNodeSet = new PrefixActiveNodeSet(newString, this->getEditDistanceThreshold(), this->trieRootNodeSharedPtr, this->supportSwapInEditDistance);

    // PAN:
    for (std::map<const TrieNode*, PivotalActiveNode >::const_iterator mapIterator = PANMap.begin();
            mapIterator != PANMap.end(); mapIterator ++) {
        // Compute the new active nodes for this trie node
        _addPANSetForOneNode(mapIterator->first, mapIterator->second, additionalChar, newActiveNodeSet);
    }

    boost::shared_ptr<PrefixActiveNodeSet> newActiveNodeSetSharedPtr;
    newActiveNodeSetSharedPtr.reset(newActiveNodeSet);
    return newActiveNodeSetSharedPtr;
}

void PrefixActiveNodeSet::printActiveNodes(const Trie* trie) const// Deprecated due to removal of TrieNode->getParent() pointers.
{
    typedef const TrieNode* trieNodeStar;
    std::map<trieNodeStar,PivotalActiveNode>::const_iterator mapIterater;
    for ( mapIterater  = this->PANMap.begin(); mapIterater != this->PANMap.end(); mapIterater++ ) {
        trieNodeStar trieNode = mapIterater->first;
        string prefix;
        trie->getPrefixString(this->trieRootNodeSharedPtr->root, trieNode, prefix);
        Logger::debug("%s : %d" , prefix.c_str(),mapIterater->second.transformationdistance );
    }
}

void PrefixActiveNodeSet::_addPANSetForOneNode(const TrieNode *trieNode, PivotalActiveNode pan,
        const CharType additionalChar, PrefixActiveNodeSet *newActiveNodeSet)
{
    // deletion
    if (additionalChar < CHARTYPE_FUZZY_UPPERBOUND) {
        PivotalActiveNode dpan;
        dpan.transformationdistance = pan.transformationdistance + 1;
        dpan.differ = pan.differ + 1;
        dpan.editdistanceofPrefix = pan.editdistanceofPrefix;
        newActiveNodeSet->_addPAN(trieNode, dpan);
    }

    // go through the children of this treNode
    int depthLimit = this->getEditDistanceThreshold() - pan.editdistanceofPrefix;//transformationdistance; LGL: A bug
    int curDepth = 0;
    addPANUpToDepth(trieNode, pan, curDepth, depthLimit, additionalChar, newActiveNodeSet);

}

void PrefixActiveNodeSet::_addPAN(const TrieNode *trieNode, PivotalActiveNode pan)
{
    if (pan.transformationdistance > this->editDistanceThreshold) // do nothing if the new distance is above the threshold
        return;
    //PAN:
    std::map<const TrieNode*, PivotalActiveNode >::iterator mapIterator = PANMap.find(trieNode);
    if (mapIterator != PANMap.end()) { // found one
        if (mapIterator->second.transformationdistance > pan.transformationdistance) // reassign the distance if it's smaller
            mapIterator->second = pan;
        else if (mapIterator->second.transformationdistance == pan.transformationdistance) {
            if ((mapIterator->second.differ < pan.differ)||(mapIterator->second.editdistanceofPrefix > pan.editdistanceofPrefix))
                mapIterator->second = pan;
        }
        return; // otherwise, do nothing
    }

    // insert the new pair
    PANMap.insert(std::pair<const TrieNode*, PivotalActiveNode >(trieNode, pan));

    // set the flag
    this->trieNodeSetVectorComputed = false;
}

void PrefixActiveNodeSet::addPANUpToDepth(const TrieNode *trieNode, PivotalActiveNode pan, const unsigned curDepth, const unsigned depthLimit, const CharType additionalChar, PrefixActiveNodeSet *newActiveNodeSet)
{
    // add children
    int max = curDepth;
    if ( max < pan.differ )
        max = pan.differ;
    PivotalActiveNode panlocal;

    // if the character is larger than or equal to the upper bound or the depthLimit == 0, we directly
    // locate the child and add it to the pans
    // When depthLimit == 0 we do an exact search, so we don't need to go in the loop
    if (additionalChar >= CHARTYPE_FUZZY_UPPERBOUND || depthLimit == 0) {
        int childPosition = trieNode->findChildNodePosition(additionalChar);
        if (childPosition >= 0) {
            const TrieNode *child= trieNode->getChild(childPosition);
            panlocal.transformationdistance = pan.editdistanceofPrefix + max;
            panlocal.differ = 0;
            panlocal.editdistanceofPrefix = pan.editdistanceofPrefix + max;
            newActiveNodeSet->_addPAN(child, panlocal);
        }
        return;
    }
    for (unsigned int childIterator = 0; childIterator < trieNode->getChildrenCount(); childIterator++) {
        const TrieNode *child = trieNode->getChild(childIterator);
        //if the current child's character is larger than the upper bound,
        // we no longer need to visit those later children.
        if (child->getCharacter() >= CHARTYPE_FUZZY_UPPERBOUND) break;
        if (child->getCharacter() == additionalChar) { // match
            panlocal.transformationdistance = pan.editdistanceofPrefix + max;
            panlocal.differ = 0;
            panlocal.editdistanceofPrefix = pan.editdistanceofPrefix + max;
            newActiveNodeSet->_addPAN(child, panlocal);
            if(supportSwapInEditDistance){
                //swap operation: if there was a delete operation, and there are prefix string
                if (pan.differ > 0 && this->prefix.size()) {
                    // if the last character of prefix can be found in curent's child, it's swap operation, we will give it the same edit distance as its parent
                    int childPosition = child->findChildNodePosition(this->prefix.back());
                    if (childPosition >= 0) {
                        child= child->getChild(childPosition);
                        panlocal.transformationdistance = pan.editdistanceofPrefix + max;
                        panlocal.differ = 0;
                        panlocal.editdistanceofPrefix = pan.editdistanceofPrefix + max;
                        newActiveNodeSet->_addPAN(child, panlocal);
                    }
                }
            }
        }
        if (curDepth < depthLimit) {// recursive call for each child
            addPANUpToDepth(child, pan, curDepth+1, depthLimit, additionalChar, newActiveNodeSet);
        }
    }
}

}
}
