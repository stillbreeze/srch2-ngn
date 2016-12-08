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
 * QuadTree.h
 *
 *  Created on: Jul 1, 2014
 */

#ifndef __QUADTREE_H__
#define __QUADTREE_H__

#include <vector>
#include <queue>
#include "geo/QuadTreeNode.h"
#include "util/mypthread.h"

using namespace std;

namespace srch2{
namespace instantsearch{

const double GEO_TOP_RIGHT_X = 200.0;    // The top right point of the maximum rectangle range of the whole quadtree
const double GEO_TOP_RIGHT_Y = 200.0;
const double GEO_BOTTOM_LEFT_X = -200.0;    // The bottom left point of the maximum rectangle range of the whole quadtree
const double GEO_BOTTOM_LEFT_Y = -200.0;

class QuadTree
{
public:
	typedef boost::shared_ptr<QuadTreeRootNodeAndFreeLists> QuadTreeRootNodeSharedPtr;

	QuadTree();

	virtual ~QuadTree();

	void deleteQuadTreeNode(QuadTreeNode* quadTreeNode);

	void getQuadTreeRootNode_ReadView(QuadTreeRootNodeSharedPtr &quadTreeRootNode_ReadView) const;

	QuadTreeNode* getQuadTreeRootNode_WriteView() const;

	// Insert a new record to the quadtree
	// Do not use after calling commit
	bool insert(const Record *record, unsigned recordInternalId);

	// Insert a new geo element to the quadtree
	// Do not use after calling commit
	bool insert(GeoElement* element);

	bool insert_ThreadSafe(const Record *record, unsigned recordInternalId);

	bool insert_ThreadSafe(Point &point, unsigned recordInternalId);

	bool insert_ThreadSafe(GeoElement* element);

	// Remove the record from the quadtree
	bool remove_ThreadSafe(Point &point, unsigned recordInternalId);

	// Remove the geo element from the quadtree
	bool remove_ThreadSafe(GeoElement* element);

	// Update a record in the quadtree
	bool update_ThreadSafe(const Record *record, unsigned recordInternalId);

	// Update a geo element in the quadtree
	bool update_ThreadSafe(GeoElement* element);

	// Find all the geo elements in the range
	void rangeQuery(vector<vector<GeoElement*>*> & results, const Shape &range, QuadTreeNode* root) const;

	// Find all the quadtree nodes inside the query range.
	// If the query's rectangle contains the rectangles of all the nodes in a subtree, it only returns the root of that subtree.
	void rangeQuery(vector<QuadTreeNode*> & results, const Shape &range, QuadTreeNode* root) const;

	unsigned getTotalNumberOfGeoElements();

	void commit();

	void merge();

	bool isMergeRequired(){
		return mergeRequired;
	}



private:
	boost::shared_ptr<QuadTreeRootNodeAndFreeLists> root_readview;
	QuadTreeNode* root_writeview;       // normal pointer to the root of the Quadtree for writeview
	mutable pthread_spinlock_t m_spinlock;

	// We keep the old read views in a queue. The goal is to make sure quad nodes in these views
	// can be freed in the order the read views were added into the queue.
	queue< boost::shared_ptr<QuadTreeRootNodeAndFreeLists> > oldReadViewQueue;

	bool commited;
	bool mergeRequired;

    friend class boost::serialization::access;

    template<class Archive>
    void save(Archive & ar, const unsigned int version) const{
    	// We do NOT need to serialize the "committed" flag since the quadtree should have committed.
    	ar << root_readview;
    }

    template<class Archive>
    void load(Archive & ar, const unsigned int version){
    	// We do NOT need to read the "committed" flag from the disk since the quadtree should have committed and the flag should true.
    	commited = true;
    	ar >> root_readview;
    	// free any old memory pointed by this->root_writeview to avoid memory leaks.
    	if(this->root_writeview)
    		delete this->root_writeview;
    	this->root_writeview = new QuadTreeNode(this->root_readview.get()->root);
    }

    template<class Archive>
    void serialize(Archive & ar, const unsigned int file_version)
    {
        boost::serialization::split_member(ar, *this, file_version);
    }

};


}
}


#endif /* __QUADTREE_H__ */
