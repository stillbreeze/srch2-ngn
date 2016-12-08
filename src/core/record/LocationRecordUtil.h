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

#ifndef LOCATIONRECORDUTIL_H_
#define LOCATIONRECORDUTIL_H_

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <cmath>
#include <vector>
#include <string>
#include <sstream>

namespace srch2
{
namespace instantsearch
{


class Point
{
public:
    double x;
    double y;

    Point()
    {
        x = 0;
        y = 0;
    }

    bool operator==(const Point &point) const
    {
        return x == point.x && y == point.y;
    };

    double distSquare(const Point &point) const
    {
        return (this->x - point.x) * (this->x - point.x)
            + (this->y - point.y) * (this->y - point.y);
    }

private:
    friend class boost::serialization::access;

    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & this->x;
        ar & this->y;
    }
};

class Rectangle;

class Shape
{
public:
    Shape() {};
    virtual ~Shape() {};

    virtual bool contain(const Point &point) const = 0;
    virtual bool contain(const Rectangle &rectangle) const = 0;
    virtual bool intersects(const Rectangle &) const = 0;
    virtual double getMinDist2FromLatLong(double resultLat, double resultLng) const = 0;
    virtual double getSearchRadius2() const = 0;
    virtual void getValues(std::vector<double> &values) const = 0;
    virtual double getMinDistFromBoundary(double lat, double lng) const = 0;
    virtual void getCenter(Point &point) = 0;
    virtual std::string toString() = 0;
};

class Rectangle : public Shape
{
public:
    // the mbr's lower value
    Point min;
    // the mbr's upper value
    Point max;

    Rectangle()
    {
        this->min.x = 0;
        this->min.y = 0;
        this->max.x = 0;
        this->max.y = 0;
    }

    Rectangle(const std::pair<std::pair<double,double>, std::pair<double,double> >& rect)
    {
        this->min.x = rect.first.first;
        this->min.y = rect.first.second;
        this->max.x = rect.second.first;
        this->max.y = rect.second.second;
    }

    virtual ~Rectangle() {}

    // check whether two rectangles intersect or not
    virtual bool intersects(const Rectangle &rect) const
    {
        return min.x <= rect.max.x && rect.min.x <= max.x
            && min.y <= rect.max.y && rect.min.y <= max.y;
    };

    // check whether the rectangle contains the point
    virtual bool contain(const Point &point) const
    {
        return min.x <= point.x && max.x >= point.x
            && min.y <= point.y && max.y >= point.y;
    };

    // check whether the rectangle contains the rectangle
    virtual bool contain(const Rectangle &rectangle) const
    {
    	return this->contain(rectangle.min) && this->contain(rectangle.max);
    };

    // check whether this rectangle is contained by another
    bool containedBy(const Rectangle &rect) const
    {
        return min.x >= rect.min.x && min.y >= rect.min.y
            && max.x <= rect.max.x && max.y <= rect.max.y;
    };

    bool operator==(const Rectangle &r) const
    {
            return min == r.min && max == r.max;
    };

    virtual double getMinDist2FromLatLong(double resultLat, double resultLng) const
    {
        double rangeMidLat = (this->max.x + this->min.x) / 2.0;
        double rangeMidLng = (this->max.y + this->min.y) / 2.0;
        return pow((resultLat - rangeMidLat), 2) + pow((resultLng - rangeMidLng), 2);
    }

    // this function will return the distance of closest point on the boundary of the shape to the input point
    virtual double getMinDistFromBoundary(double lat, double lng) const
    {
    	Point point;
    	point.x = lat;
    	point.y = lng;
    	if(this->contain(point))
    		return 0;
    	double xDist =  std::min(abs(max.x - lat), abs(min.x - lat));
    	double yDist =  std::min(abs(max.y - lng), abs(min.y - lng));
    	return sqrt(xDist * xDist + yDist * yDist);
    }

    // this function will return the center of the shape
    virtual void getCenter(Point &point){
    	point.x = (this->max.x + this->min.x) / 2.0;
    	point.y = (this->max.y + this->min.y) / 2.0;
    }

    virtual double getSearchRadius2() const
    {
        return pow((this->max.x - this->min.x)/2.0, 2) + pow((this->max.y - this->min.y)/2.0, 2);
    }

    virtual void getValues(std::vector<double> &values) const
    {
        values.push_back(this->min.x);
        values.push_back(this->min.y);
        values.push_back(this->max.x);
        values.push_back(this->max.y);
    }

    virtual std::string toString(){
    	std::ostringstream ss;
    	ss << "Rectangle";
    	ss << "/" << this->min.x << "/" << this->min.y;
    	ss << "/" << this->max.x << "/" << this->max.y;
    	return ss.str();
    }

private:
    friend class boost::serialization::access;

    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & this->min;
        ar & this->max;
    }
};

class Circle : public Shape
{
private:
    Point center;
    double radius;
public:
    Circle(Point c, double r) : center(c), radius(r) {}

    virtual ~Circle() {}

    virtual bool contain(const Point &point) const
    {
        return center.distSquare(point) <= radius*radius;
    }

    // check whether the Circle contains the rectangle
    virtual bool contain(const Rectangle &rectangle) const
    {
    	Point point1;
    	point1.x = rectangle.max.x;
    	point1.y = rectangle.min.y;
    	Point point2;
    	point2.x = rectangle.min.x;
    	point2.y = rectangle.max.y;
    	return this->contain(point1) && this->contain(point2) && this->contain(rectangle.max) && this->contain(rectangle.min);
    }

    // how to detect the intersection between a circle and a rectangle
    // http://stackoverflow.com/a/402010/1407156
    virtual bool intersects(const Rectangle &rect) const
    {
        double rectX = (rect.min.x + rect.max.x) / 2;
        double rectY = (rect.min.y + rect.max.y) / 2;
        double width = rect.max.x - rect.min.x;
        double height = rect.max.y - rect.min.y;

        double distanceX = ::abs(center.x - rectX);
        double distanceY = ::abs(center.y - rectY);

        if (distanceX > (width/2 + radius)) { return false; }
        if (distanceY > (height/2 + radius)) { return false; }

        if (distanceX <= (width/2)) { return true; } 
        if (distanceY <= (height/2)) { return true; }

        double cornerDistance_sq = (distanceX - width/2) * (distanceX - width/2) +
                                   (distanceY - height/2) * (distanceY - height/2);

        return (cornerDistance_sq <= (radius*radius));
    }

    virtual double getMinDist2FromLatLong(double resultLat, double resultLng) const
    {
        return pow((resultLat - center.x), 2) + pow((resultLng - center.y), 2);
    }

    // this function will return the distance of closest point on the boundary of the shape to the input point
    virtual double getMinDistFromBoundary(double lat, double lng) const
    {
    	Point point;
    	point.x = lat;
    	point.y = lng;
    	if(this->contain(point))
    		return 0;
    	return sqrt(this->getMinDist2FromLatLong(lat,lng)) - this->radius;
    }

    // this function will return the center of the shape
    virtual void getCenter(Point &point){
    	point.x = this->center.x;
    	point.y = this->center.y;
    }

    virtual double getSearchRadius2() const
    {
        return pow(radius, 2);
    }

    virtual void getValues(std::vector<double> &values) const
    {
        values.push_back(center.x);
        values.push_back(center.y);
        values.push_back(radius);
    }

    virtual std::string toString(){
    	std::ostringstream ss;
    	ss << "Circle";
    	ss << "/" << this->center.x;
    	ss << "/" << this->center.y;
    	ss << "/" << this->radius;
    	return ss.str();
    }

};

}}

#endif /* LOCATIONRECORDUTIL_H_ */
