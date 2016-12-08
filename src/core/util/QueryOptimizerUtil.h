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

#ifndef __SRCH2_UTIL_QUERYOPTIMIZERUTIL_H__
#define __SRCH2_UTIL_QUERYOPTIMIZERUTIL_H__

#include <iostream>
using namespace std;

namespace srch2 {
namespace util {

class QueryOptimizerUtil {
public:
	/*
	 * Example :
	 * domains :             1,3,3
	 * currentProduct :      0,1,2
	 * nextProduct will be : 0,2,2 (0+1 !< 1 so gives us a carry which increments the middle 1 to 2)
	 */
	static void increment(unsigned numberOfDomains, unsigned * domains, unsigned * currentProduct , unsigned * outputProduct){

		unsigned carry = 1; // because we want to increment

		for(unsigned d = 0 ; d < numberOfDomains ; ++d){
			unsigned outputDigit = 0;
			if(currentProduct[d] + carry < domains[d]){
				outputDigit = currentProduct[d] + carry;
				carry = 0;
			}else{
				carry = 1;
			}
			outputProduct[d] = outputDigit;
		}
	}

	/*
	 * Example :
	 * numberOfDomains = 3;
	 * domains = [2,3,1]
	 * catProductResults should be [0,0,0 ,0,1,0 ,0,2,0 ,1,0,0 ,1,1,0 ,1,2,0]
	 */
	static void cartesianProduct(unsigned numberOfDomains, unsigned * domains, unsigned * cartProductResults , unsigned totalNumberOfProducts){
		// initialize the first product (all zero)
		for(unsigned d = 0; d < numberOfDomains ; ++d){
			cartProductResults[d] = 0;
		}
		unsigned * currentProduct = cartProductResults;
		for(unsigned p = 0; p < totalNumberOfProducts - 1 ; ++p){
			increment(numberOfDomains,domains,currentProduct + (numberOfDomains * p) , currentProduct + (numberOfDomains * (p+1)) );
		}
	}

	static void printIndentations(unsigned indent){
		if(indent < 1) return;
		for(int i=0;i<indent-1;i++){
			Logger::info("\t");
		}
		Logger::info("-\t");
	}

};


}
}


#endif // __SRCH2_UTIL_QUERYOPTIMIZERUTIL_H__
