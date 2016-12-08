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

#include <instantsearch/Ranker.h>
#include "util/Assert.h"
#include "util/AttributeIterator.h"
#include "geo/QuadTreeNode.h"
#include <iostream>
#include <math.h>
#include <cfloat>

using std::vector;

namespace srch2
{
    namespace instantsearch
    {
    // Since the tf * sumOfFieldBoosts never changes during the life cycle of a record, this value
    // is saved into the forward list so that the engine does not need to compute tf and
    // sumOfFieldBoosts repeatedly.
    float DefaultTopKRanker::computeTextRelevance(const float tfBoostProdcut, const float idf)
    {
        return tfBoostProdcut * idf;
    }

    //Calculate the product of tf and sumOfFieldBoosts.
    float DefaultTopKRanker::computeRecordTfBoostProdcut(const float tf, const float sumOfFieldBoosts)
    {
        return tf * sumOfFieldBoosts;
    }

    // this function is used in the term virtual list (heap) to compute the score of
    // a record with respect to a keyword 
    float DefaultTopKRanker::computeTermRecordRuntimeScore(float termRecordStaticScore, 
                                   unsigned editDistance, unsigned termLength, 
                                   bool isPrefixMatch,
                                   float prefixMatchPenalty , float termSimilarityBoost)
    {
        unsigned ed = editDistance;
        if (ed > termLength)
        	ed = termLength;

        // TODO : change this power calculation to using a pre-computed array
        float normalizedEdSimilarity = (1 - (1.0*ed) / termLength)* pow(termSimilarityBoost, (float) ed);
        float PrefixMatchingNormalizer = isPrefixMatch ? (prefixMatchPenalty) : 1.0;
        return termRecordStaticScore * normalizedEdSimilarity * PrefixMatchingNormalizer;
    }
    
    float DefaultTopKRanker::computeOverallRecordScore(const Query *query, 
                               const vector<float> &queryResultTermScores) const
    {
        const vector<Term *> *queryTerms = query->getQueryTerms();
        ASSERT(queryTerms->size() == queryResultTermScores.size());
        
        float overallScore = 0.0;
        vector<float>::const_iterator queryResultsIterator = queryResultTermScores.begin();
        for (vector<Term *>::const_iterator queryTermsIterator = queryTerms->begin(); 
         queryTermsIterator != queryTerms->end(); queryTermsIterator++ , queryResultsIterator++ ) {
            float termRecordRuntimeScore =  (*queryResultsIterator);
            // apply the aggregation function
            overallScore = DefaultTopKRanker::aggregateBoostedTermRuntimeScore(overallScore,
                                           (*queryTermsIterator)->getBoost(),
                                           termRecordRuntimeScore);
        }

        return overallScore;
    }

    // aggregation function used in top-k queries (Fagin's algorithm)
    float DefaultTopKRanker::aggregateBoostedTermRuntimeScore(float oldRecordScore,
                                  float termBoost,
                                  float termRecordRuntimeScore) const
    {
        // We use the "addition" function to do the aggregation
        float newRecordScore = oldRecordScore + termBoost * termRecordRuntimeScore;
        return newRecordScore;
    }

    bool DefaultTopKRanker::compareRecordsLessThan(float leftRecordScore,  unsigned leftRecordId,
                               float rightRecordScore, unsigned rightRecordId)
    {
        if (leftRecordScore == rightRecordScore)
        return leftRecordId > rightRecordId;  // earlier records are ranked higher
        else
        return leftRecordScore < rightRecordScore;
    }

    bool DefaultTopKRanker::compareRecordsGreaterThan(float leftRecordScore,  unsigned leftRecordId,
                              float rightRecordScore, unsigned rightRecordId)
    {
        if (leftRecordScore == rightRecordScore)
        return leftRecordId < rightRecordId; 
        else
        return leftRecordScore > rightRecordScore;
    }

    bool DefaultTopKRanker::compareRecordsGreaterThan(const TypedValue & leftRecordTypedValue,  unsigned leftRecordId,
    		const TypedValue & rightRecordTypedValue, unsigned rightRecordId)
    {
    	if (leftRecordTypedValue == rightRecordTypedValue)
    		return leftRecordId < rightRecordId;
    	else
    		return leftRecordTypedValue > rightRecordTypedValue;
    }


    float DefaultTopKRanker::computeAggregatedRuntimeScoreForAnd(std::vector<float> runTimeTermRecordScores){

    	float resultScore = 0;

    	for(vector<float>::iterator score = runTimeTermRecordScores.begin(); score != runTimeTermRecordScores.end(); ++score){
    		resultScore += *(score);
    	}
    	return resultScore;
    }

    float DefaultTopKRanker::computeAggregatedRuntimeScoreForOr(std::vector<float> runTimeTermRecordScores){

    	// max
    	float resultScore = -1;

    	for(vector<float>::iterator score = runTimeTermRecordScores.begin(); score != runTimeTermRecordScores.end(); ++score){
    		if((*score) > resultScore){
    			resultScore = (*score);
    		}
    	}
    	return resultScore;
    }

    float DefaultTopKRanker::computeScoreForNot(float score){
    	return 1 - score;
    }

    double DefaultTopKRanker::computeScoreforGeo(Point &recordPosition, Shape &queryShape){
    	// calculate the score
    	double minDist2UpperBound = max( queryShape.getSearchRadius2() , GEO_MIN_SEARCH_RANGE_SQUARE);
    	double resultMinDist2 = queryShape.getMinDist2FromLatLong(recordPosition.x, recordPosition.y);
    	double distanceRatio = ((double)sqrt(minDist2UpperBound) - (double)sqrt(resultMinDist2)) / (double)sqrt(minDist2UpperBound);
    	return max( distanceRatio * distanceRatio, GEO_MIN_DISTANCE_SCORE );
    }

//    float DefaultTopKRanker::computeFeedbackBoost(unsigned feedbackRecencyInSecs, unsigned feedbackFrequency) {
//		/*
//		 *  Feedback boost for a record found in the user feedback list for a query is
//		 *  calculated as.
//		 *                           1
//		 *   FeedbackBoost = 1 + -------------  X  square root (f)
//		 *                       1 + (t1 - t2)
//		 *
//		 *   Where t1 = timestamp of query arrival ( time of creation of this operator).
//		 *         t2 = most recent timestamp for a record marked as a feedback for this query.
//		 *         f  = number of times a record was submitted as a feedback for this query.
//		 */
//    	float feedbackBoost = 0.0;
//		float recencyFactor = 1.0 / (1.0 + ((float)(feedbackRecencyInSecs) / 60.0));
//		float frequencyFactor = sqrtf(feedbackFrequency);
//		feedbackBoost = 1.0 + recencyFactor * frequencyFactor;
//		return feedbackBoost;
//    }
    float DefaultTopKRanker::computeFeedbackBoost(unsigned feedbackRecencyInSecs, unsigned feedbackFrequency) {
    	/*
    	 *  Feedback boost for a record found in the user feedback list for a query is
    	 *  calculated as.
    	 *
    	 *   FeedbackBoost = 1 +     recency factor       *  frequency factor
    	 *
    	 *                 = 1 +  (1 - ((t1- t2)/ K)^2 )  *  square root (f)
    	 *
    	 *
    	 *   Where t1 = timestamp of query arrival ( time of creation of this operator).
    	 *         t2 = most recent timestamp for a record marked as a feedback for this query.
    	 *         f  = number of times a record was submitted as a feedback for this query.
    	 *         K =  Constant factor deciding Timespan in second when recency factor will be 0.
    	 *              We choose 3 months. i.e K = (3 * 30 * 24 * 60 * 60) = 7776000
    	 *              recency factor is inverted square curve (1-x^2). It decays slowly compared to
    	 *              inverse function (1/x) or linear function (1-x).
    	 *              Google "1 - ( x / 7776000)^2" to see the curve.
    	 *
    	 */
    	float feedbackBoost = 0.0;
    	float recencyFactor = 1.0 - pow(((float)feedbackRecencyInSecs / 7776000.0), 2);
    	float frequencyFactor = sqrtf(feedbackFrequency);
    	feedbackBoost = 1.0 + recencyFactor * frequencyFactor;
    	return feedbackBoost;
    }
    // compute the boosted score. original score * feedback boost.
    float DefaultTopKRanker::computeFeedbackBoostedScore(float score, float boost) {
    	return score * boost;
    }

    /*float DefaultTopKRanker::computeOverallRecordScore(const Query *query, const vector<float> &queryResultTermScores, unsigned recordLength) const
      {
      const vector<Term *> *queryTerms = query->getQueryTerms();
      
      ASSERT(queryTerms->size() == queryResultTermScores.size());
      
      float overallScore = 0.0;
      vector<float>::const_iterator queryResultsIterator = queryResultTermScores.begin();
      for ( vector<Term *>::const_iterator queryTermsIterator = queryTerms->begin(); queryTermsIterator != queryTerms->end(); queryTermsIterator++ , queryResultsIterator++ )
    {
    overallScore += computeTermWeightedRecordScore(*queryTermsIterator, *queryResultsIterator);
    }
    
    // we add the boost value based on the record length
    overallScore = overallScore + ( (1 / (float)(recordLength + 4)) * query->getLengthBoost());
    
    return overallScore;
    }*/

//TODO: BUG: Make length const after fixing bug ticket 36
/*float DefaultTopKRanker::computeSimilarity(const Term *term, const float invertedListElementScore, const unsigned distance, const unsigned length) const
{
    float returnValue;
    switch(distance)
    {
        case 0:
               returnValue = ((invertedListElementScore ))/(float)(length+1);
               break;
        case 1:
            returnValue = (invertedListElementScore * term->getSimilarityBoost())/(float)(4*(length+1));
            break;
        case 2:
            returnValue = (invertedListElementScore * term->getSimilarityBoost())/(float)(16*(length+1));
            break;
        case 3:
            returnValue = (invertedListElementScore * term->getSimilarityBoost())/(float)(32*(length+1));
            break;
        default:
            returnValue = (invertedListElementScore * term->getSimilarityBoost())/(float)(64*(length+1));
            break;
     }
    return returnValue;

    float returnValue = 0.0;
    //float fuzzySimilarityScore = (1-((float)distance/(float)length));
    float fuzzySimilarityScore = (1-( (float)distance/(float)(term->getKeyword()->length()) ));
    float w = term->getSimilarityBoost();
    returnValue = ( w * fuzzySimilarityScore ) + ( (1 - w) * invertedListElementScore);
    return returnValue;
}*/


float GetAllResultsRanker::computeResultScoreUsingAttributeScore(const Query *query, const float recordAttributeScore, const unsigned totalEditDistance, const unsigned totalQueryLength) const
{
    const vector<Term *> *queryTerms = query->getQueryTerms();
    float returnValue = 0.0;
    float fuzzySimilarityScore = (1-((float)totalEditDistance/(float)totalQueryLength));
    
    for ( vector<Term *>::const_iterator queryTermsIterator = queryTerms->begin(); 
      queryTermsIterator != queryTerms->end(); queryTermsIterator++ ) {
    const Term *term = *queryTermsIterator;
    float w = term->getSimilarityBoost();
    returnValue += ( w * fuzzySimilarityScore ) + ( (1 - w) * recordAttributeScore );
    break;
    }
    return returnValue;
}

double SpatialRanker::getDistanceRatio(const double minDist2UpperBound, const double resultMinDist2 ) const
{
    return ((double)sqrt(minDist2UpperBound) - (double)sqrt(resultMinDist2)) / (double)sqrt(minDist2UpperBound);
}

// uses the function in Geo
// two changes are made because we use range query here instead of kNN query
//    1. the "query point" is set to the center point of the query range
//    2. instead of using a fixed minDist2UpperBound, we need to set it according to the range's radius
//     the "search radius" is set to half of the diagonal
float SpatialRanker::combineKeywordScoreWithDistanceScore(const float keywordScore, const float distanceScore) const
{
    return distanceScore * keywordScore;
}

double SpatialRanker::calculateHaversineDistanceBetweenTwoCoordinates(double latitude1, double longitude1, double latitude2, double longitude2) const
{
    double radius = 6371.0; // radius of earth in kms
    double dLat = degreeToRadian(latitude2-latitude1);
    double dLon = degreeToRadian(longitude2-longitude1);
    double lat1 = degreeToRadian(latitude1);
    double lat2 = degreeToRadian(latitude2);
    double alpha = sin(dLat/2.0) * sin(dLat/2.0) + sin(dLon/2.0) * sin(dLon/2.0) * cos(lat1) * cos(lat2);
    double angularDistance = 2.0 * atan2(sqrt(alpha), sqrt(1.0 - alpha));
    double distanceInKilometers = radius * angularDistance;
    double distanceInMiles = distanceInKilometers * 0.6214;

    return distanceInMiles;
}

// Convert degree value to radian value using the formula : radian = degree*PI/180
double SpatialRanker::degreeToRadian(double degreeValue) const
{
    const double PI = 3.1415926535;
    return degreeValue * PI / 180.0;
}

uint8_t computeEditDistanceThreshold(unsigned keywordLength , float similarityThreshold)
{
	if(similarityThreshold < 0 || similarityThreshold > 1) {
		ASSERT(false);
		return 0;
	}
	// We add "FLT_EPSILON" to deal with imprecise representations of float.
	float fresult = keywordLength * (1 - similarityThreshold + FLT_EPSILON);
	return fresult; // casting to unsigned int will do the floor operation automatically.
}

// Solr's ranking function: https://lucene.apache.org/core/2_9_4/api/all/org/apache/lucene/search/Similarity.html
// This explains the essence of sloppy frequency and how it is used in ranking function

// http://lucene.apache.org/core/4_0_0/core/org/apache/lucene/search/similarities/DefaultSimilarity.html
// This explains how the function to calculate sloppy frequency is implemented

float DefaultTopKRanker::computeSloppyFrequency(const vector<unsigned>& listOfSlopDistances) const{

    float sum = 0;
    for(int i = 0; i < listOfSlopDistances.size(); i++){
        sum = sum + 1.0/(1.0+listOfSlopDistances[i]);
    }
    return sqrt(sum);
}

// It computes the run time score of phrase operator by multiplying runtime score obtained from AND operator and sloppyFreqency of the phrase.
float DefaultTopKRanker::computePositionalScore(float runtimeScore, float sloppyFrequency) const{

    return runtimeScore * sloppyFrequency;
}

}}
