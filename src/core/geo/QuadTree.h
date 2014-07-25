/*
 * QuadTree.h
 *
 *  Created on: Jul 1, 2014
 *      Author: mahdi
 */

#ifndef __QUADTREE_H__
#define __QUADTREE_H__

#include <vector>

#include "geo/QuadTreeNode.h"

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
	QuadTree();

	virtual ~QuadTree();

	// Insert a new record to the quadtree
	bool insert(const Record *record, unsigned recordInternalId);

	// Insert a new geo element to the quadtree
	bool insert(GeoElement* element);

	// Remove the record from the quadtree
	bool remove(const Record *record, unsigned recordInternalId);

	// Remove the geo element from the quadtree
	bool remove(GeoElement* element);

	// Update a record in the quadtree
	bool update(const Record *record, unsigned recordInternalId);

	// Update a geo element in the quadtree
	bool update(GeoElement* element);

	// Find all the geo elements in the range
	void rangeQuery(vector<vector<GeoElement*>*> & results, const Shape &range) const;

	// Find all the quadtree nodes inside the query range.
	// If the query's rectangle contains the rectangles of all the nodes in a subtree, it only returns the root of that subtree.
	void rangeQuery(vector<QuadTreeNode*> & results, const Shape &range) const;

	unsigned getTotalNumberOfGeoElements(){
		return this->root->getNumOfElementsInSubtree();
	}

	QuadTreeNode* getRoot(){
		return this->root;
	};

	bool equalTo(QuadTree* quadtree);

private:
	QuadTreeNode* root;    // Pointer to the root of the Quadtree.

    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & this->root;
    }

};


}
}


#endif /* __QUADTREE_H__ */
