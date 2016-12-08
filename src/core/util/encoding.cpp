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
//
// encoding.cpp
//
//  Created on: 2013-4-6
//

#include "encoding.h"
#include "Logger.h"
using srch2::util::Logger;


// Tranform a utf-8 string to a CharType vector.
void utf8StringToCharTypeVector(const string &utf8String, vector<CharType> & charTypeVector)
{
	charTypeVector.clear();

	// We check if the string is a valid utf-8 string first. If not, we get a valid utf-8 prefix of this string.
	// Then we transform the string to CharType vector.
	// For example, suppose utf8String = c_1 c_2 c_3 c_4 c_5, where each c_i represents a character.
	// Assume that c_4 is not a valid utf-8 character since it doesn't conform the utf-8 encoding scheme.
	// Then we will get the valid utf-8 prefix c_1 c_2 c_3.  We transform it to a CharType vector
	// and return it.
	string::const_iterator end_it = utf8::find_invalid(utf8String.begin(), utf8String.end());
	if (end_it != utf8String.end()) {
        Logger::warn("Invalid UTF-8 encoding detected in %s", utf8String.c_str());
	}

	utf8::utf8to32(utf8String.begin(), end_it, back_inserter(charTypeVector));
}
void charTypeVectorToUtf8String(const vector<CharType> &charTypeVector, string &utf8String)
{
	utf8String.clear();
	utf8::utf32to8(charTypeVector.begin(), charTypeVector.end(), back_inserter(utf8String));
}

void charTypeVectorToUtf8String(const vector<CharType> &charTypeVector, int begin, int end, string &utf8String){
    utf8String.clear();
    utf8::utf32to8(charTypeVector.begin()+begin, charTypeVector.begin()+end, 
            back_inserter(utf8String));
}

vector<CharType> getCharTypeVector(const string &utf8String)
{
	vector<CharType> charTypeVector;
	utf8StringToCharTypeVector(utf8String, charTypeVector);
	return charTypeVector;
}
string getUtf8String(const vector<CharType> &charTypeVector)
{
	string utf8String;
	charTypeVectorToUtf8String(charTypeVector, utf8String);
	return utf8String;
}

// Give a utf8String, return its number of unicode endpoints as its length
// For example "vidéo" strlen will return 6, this function will return 5
const unsigned getUtf8StringCharacterNumber(const string utf8String)
{
	string::const_iterator end_it = utf8::find_invalid(utf8String.begin(), utf8String.end());
	if (end_it != utf8String.end()) {
        Logger::warn("Invalid UTF-8 encoding detected in %s", utf8String.c_str());
	}
	// Get the utf8 string length in the valid range
	// For example a_1 a_2...a_i...a_n is a string, in which a_i is not valid, we will only return i-1[a_1 a_2...a_i-1]as the length
	return utf8::distance(utf8String.begin(), end_it);
}


ostream& operator<<(ostream& out, const vector<CharType>& charTypeVector)
{
   for(vector<CharType>::const_iterator it = charTypeVector.begin(); it != charTypeVector.end(); it++)
	   out<<*it;
   return out;
}

bool operator<(const vector<CharType> &x, const vector<CharType> &y)
{
 int i = 0;
 int xlen = x.size();
 int ylen = y.size();
 while(i != xlen && i != ylen)
 {
	 if(x[i] == y[i])
		 i++;
	 else if(x[i] < y[i])
		 return true;
	 else
		 return false;
 }
 if(xlen < ylen) return true;
 else return false;
}

bool operator==(const vector<CharType> &x, const vector<CharType> &y)
{
	if(x.size() != y.size()) return false;
	else
	{
		for(int i = 0; i< x.size(); i++)
		{
			if(x[i] != y[i])
				return false;
		}
		return true;
	}
}
