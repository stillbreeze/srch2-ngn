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
 *
 * StemmerFilter.cpp
 *
 *  Created on: Jun 7, 2013
 */


#include "StemmerFilter.h"

#include <string.h>
#include <fstream>
#include "util/encoding.h"
#include "util/Logger.h"
#include "analyzer/AnalyzerContainers.h"

using namespace std;
using srch2::util::Logger;

namespace srch2 {
namespace instantsearch {

// TODO: width limit 80 chars
StemmerFilter::StemmerFilter(TokenStream* tokenStream, const StemmerContainer *_stemmerContainer) :
    TokenFilter(tokenStream), stemmerContainer(_stemmerContainer)
{
    // Based on StemmerType value it should be decided to use PORTER or MIRROR or ...

    // copies the shared_ptr: sharedToken
    this->tokenStreamContainer = tokenStream->tokenStreamContainer;
}

StemmerFilter::~StemmerFilter() {
}

//  search if the given string is present in the DictionaryWords
bool StemmerFilter::searchWord(const std::string &searchWord) const {
	return stemmerContainer->contains(searchWord);
}

/* Stems the input Token to be stemmed
 * Output is the StemmedToken
 */
std::string StemmerFilter::stemToken(const std::string &token) const {
	//	If we found the token in the dictionary, we will not change it, else we go through the porter stemmer.
	if (searchWord(token)) {
		return token;
	}

	// Calling the stem function to apply the porter rules
	return StemmerFilterInternal::stemUsingPorterRules(token);
}

// incrementToken() is a virtual function of class TokenOperator. Here we have to implement it. It goes on all tokens.
bool StemmerFilter::processToken() {
	if (this->tokenStream->processToken()) {
		// It checks that if all characters are English or not.
		for (int i = 0; i < tokenStreamContainer->currentToken.size(); i++) {
			// For each token, it goes on all characters and checks if each of the characters is between A-z or not.
			if (!((tokenStreamContainer->currentToken[i] >= (CharType) 'A')
					&& (tokenStreamContainer->currentToken[i] <= (CharType) 'z'))) {
				return true;
			}
		}
		// TODO: remove "charTypeVectorToUtf8String()"
		std::string stemmedToken = "";
		// converts the charType to string
		charTypeVectorToUtf8String(tokenStreamContainer->currentToken, stemmedToken);
		// calls the stemToken to stem
		stemmedToken = stemToken(stemmedToken);
		// converts the string to charType
		utf8StringToCharTypeVector(stemmedToken, tokenStreamContainer->currentToken);
		return true;
	} else {
		return false;
	}
}

/***********************************************************************************
 * StemmerFilterInternal Function http://tartarus.org/~martin/PorterStemmer/
 */

// If the stemmedWordBuffer[i] is a consonant it returns 1, else returns 0.
int StemmerFilterInternal::ifConsonant(struct TokenDetails * tokenDetail,
		int index) {
	//	checking the character with the non-consonant characters.
	switch (tokenDetail->stemmedWordBuffer[index]) {
	case 'a':
	case 'e':
	case 'i':
	case 'o':
	case 'u':
		return FALSE; // FALSE is equal to 0
	case 'y':
		return (index == 0) ? TRUE : !ifConsonant(tokenDetail, index - 1);
	default:
		return TRUE; // TRUE is equal to 1
	}
}

/* m() measures the number of consonant sequences between k0 and j. if c is
 a consonant sequence and v a vowel sequence, and <..> indicates arbitrary
 presence,

 <c><v>       gives 0
 <c>vc<v>     gives 1
 <c>vcvc<v>   gives 2
 <c>vcvcvc<v> gives 3
 */
int StemmerFilterInternal::numberOfConsonantSequences(
		struct TokenDetails * tokenDetailStruct) {
	//	numberOfConsonants is the number of the results
	int numberOfConsonants = 0;
	//	it is the current index on the token
	int currentIndex = 0;
	//	it is offset of the tokenDetailStruct
	int stringOffset = tokenDetailStruct->stringOffset;
	while (true) {
		// checks if the current index passed the offset, then we are done
		if (currentIndex > stringOffset) {
			return numberOfConsonants;
		}
		// if the currentIndex is not consonant, it breaks
		if (!ifConsonant(tokenDetailStruct, currentIndex)) {
			break;
		}
		currentIndex++;
	}
	currentIndex++;
	// this while continues till we get to the end of the string
	while (TRUE) {
		// this while continues till we get to a consonant
		while (TRUE) {
			// checks if the current index passed the offset, then we are done
			if (currentIndex > stringOffset) {
				return numberOfConsonants;
			}
			// if the currentIndex is consonant, it breaks
			if (ifConsonant(tokenDetailStruct, currentIndex)) {
				break;
			}
			currentIndex++;
		}
		currentIndex++;
		numberOfConsonants++;
		// this while continues till we get to a non-consonant
		while (TRUE) {
			// checks if the current index passed the offset, then we are done
			if (currentIndex > stringOffset) {
				return numberOfConsonants;
			}
			// if the currentIndex is not consonant, it breaks
			if (!ifConsonant(tokenDetailStruct, currentIndex)) {
				break;
			}
			currentIndex++;
		}
		currentIndex++;
	}
	return 0; // we will not get here and it is just for avoiding the warnings
}

// vowelInStem checks if there is any vowel in the stem or not (TRUE <=> k0,...stringOffset contains a vowel)
int StemmerFilterInternal::vowelInStem(
		struct TokenDetails * tokenDetailStruct) {
	//	strOffset is holding the stringOffset
	int strOffset = tokenDetailStruct->stringOffset;
	//	iterates on all characters and calls the ifConsonant function for each
	for (int i = 0; i <= strOffset; i++) {
		if (!ifConsonant(tokenDetailStruct, i)) {
			return TRUE; // TRUE is equal to 1
		}
	}
	return FALSE; // False is equal to 0
}

//	checks if there is a double consonant character at index and index-1 place
int StemmerFilterInternal::doubleConsonant(
		struct TokenDetails * tokenDetailStruct, int index) {
	char * buff = tokenDetailStruct->stemmedWordBuffer;
	//	if index < 1, we can not have the double Consonant
	if (index < 1) {
		return FALSE; // FALSE is equal to 0
	}
	if (buff[index] != buff[index - 1]) {
		return FALSE; // FALSE is equal to 0
	}
	//	returns if there is consonant char at the position index
	return ifConsonant(tokenDetailStruct, index);
}

/* ConsonantVowelConsonantStructure(i) is TRUE <=> i-2,i-1,i has the form consonant - vowel - consonant
 and also if the second c is not w,x or y. this is used when trying to
 restore an e at the end of a short word. e.g.
 cav(e), lov(e), hop(e), crim(e), but
 snow, box, tray.
 */
int StemmerFilterInternal::ConsonantVowelConsonantStructure(
		struct TokenDetails * tokenDetailStruct, int index) {
	//	checks if chars at the positions are vowels or consonant
	if (index < 2 || !ifConsonant(tokenDetailStruct, index)
			|| ifConsonant(tokenDetailStruct, index - 1)
			|| !ifConsonant(tokenDetailStruct, index - 2)) {
		return FALSE; // FALSE is equal to 0
	}
	//	checks the character at the position index and compares them with 'w', 'x' and 'y'
	int ch = tokenDetailStruct->stemmedWordBuffer[index];
	if (ch == 'w' || ch == 'x' || ch == 'y') {
		return FALSE;
	}
	return TRUE;
}

/* ends(s) is TRUE <=> k0,...endStringOffset ends with the string s. */
// checks if the str ends with str
int StemmerFilterInternal::endStr(struct TokenDetails * tokenDetailStruct,
		const char * str) {
	int length = str[0];
	char * buff = tokenDetailStruct->stemmedWordBuffer;
	int endStringOffset = tokenDetailStruct->endStringOffset;

	if (str[length] != buff[endStringOffset]) {
		return FALSE;
	}
	if (length > endStringOffset + 1) {
		return FALSE;
	}
	if (memcmp(buff + endStringOffset - length + 1, str + 1, length) != 0) {
		return FALSE;
	}
	tokenDetailStruct->stringOffset = endStringOffset - length;
	return TRUE;
}

//setto(s) sets (stringOffset+1),...endStringOffset to the characters in the string str
void StemmerFilterInternal::replaceBufferWithString(
		struct TokenDetails * tokenDetailStruct, const char * str) {
	int length = str[0];
	// j is the string offset
	int j = tokenDetailStruct->stringOffset;
	// copies the str starting from the offset
	memmove(tokenDetailStruct->stemmedWordBuffer + j + 1, str + 1, length);
	// sets the end offset to "current offset" + "str length"
	tokenDetailStruct->endStringOffset = j + length;
}

// depending on if there is consonant sequences or not, calls setto function.
void StemmerFilterInternal::callReplaceBufferWithStringoIfConsonant(
		struct TokenDetails * tokenDetailStruct, const char * str) {
	// if number of constant sequences greater than zero it calls the setto function.
	if (numberOfConsonantSequences(tokenDetailStruct) > 0)
		replaceBufferWithString(tokenDetailStruct, str);
}

/* step1ab() gets rid of plurals and -ed or -ing. e.g.

 s1:	   caresses  ->  caress
 s2:	   ponies    ->  poni
 s3:	   ties      ->  ti
 s4:	   caress    ->  caress
 s5:	   cats      ->  cat

 s6:	   feed      ->  feed
 s7:	   agreed    ->  agree
 s8:	   disabled  ->  disable

 s9:	   matting   ->  mat
 s10:	   mating    ->  mate
 s11:	   meeting   ->  meet
 s12:	   milling   ->  mill
 s13:	   messing   ->  mess

 meetings  ->  meet
 */

void StemmerFilterInternal::stepOneAB(struct TokenDetails * tokenDetailStruct) {
	char * buff = tokenDetailStruct->stemmedWordBuffer;
	if (buff[tokenDetailStruct->endStringOffset] == 's') {
		if (endStr(tokenDetailStruct, "\04" "sses")) //s1
			tokenDetailStruct->endStringOffset -= 2;
		else if (endStr(tokenDetailStruct, "\03" "ies")) //s2, s3
			replaceBufferWithString(tokenDetailStruct, "\01" "i");
		else if (buff[tokenDetailStruct->endStringOffset - 1] != 's') //s5
			tokenDetailStruct->endStringOffset--;
	}
	if (endStr(tokenDetailStruct, "\03" "eed")) {
		if (numberOfConsonantSequences(tokenDetailStruct) > 0) //s6
			tokenDetailStruct->endStringOffset--;
	} else if ((endStr(tokenDetailStruct, "\02" "ed")
			|| endStr(tokenDetailStruct, "\03" "ing"))
			&& vowelInStem(tokenDetailStruct)) //s11
					{
		tokenDetailStruct->endStringOffset = tokenDetailStruct->stringOffset;
		if (endStr(tokenDetailStruct, "\02" "at"))
			replaceBufferWithString(tokenDetailStruct, "\03" "ate");
		else if (endStr(tokenDetailStruct, "\02" "bl"))
			replaceBufferWithString(tokenDetailStruct, "\03" "ble");
		else if (endStr(tokenDetailStruct, "\02" "iz"))
			replaceBufferWithString(tokenDetailStruct, "\03" "ize");
		else if (doubleConsonant(tokenDetailStruct,
				tokenDetailStruct->endStringOffset)) {
			tokenDetailStruct->endStringOffset--;
			{
				int ch = buff[tokenDetailStruct->endStringOffset];
				if (ch == 'l' || ch == 's' || ch == 'z')
					tokenDetailStruct->endStringOffset++;
			}
		} else if (numberOfConsonantSequences(tokenDetailStruct) == 1
				&& ConsonantVowelConsonantStructure(tokenDetailStruct,
						tokenDetailStruct->endStringOffset))
			replaceBufferWithString(tokenDetailStruct, "\01" "e");
	}
}

/* step1c() turns terminal y to i when there is another vowel in the stem. */
void StemmerFilterInternal::stepOneC(struct TokenDetails * tokenDetailStruct) {
	if (endStr(tokenDetailStruct, "\01" "y") && vowelInStem(tokenDetailStruct))
		tokenDetailStruct->stemmedWordBuffer[tokenDetailStruct->endStringOffset] =
				'i';
}

/* step2() maps double suffices to single ones. so -ization ( = -ize plus
 -ation) maps to -ize etc. note that the string before the suffix must give
 m() > 0.
 */
void StemmerFilterInternal::stepTwo(struct TokenDetails * tokenDetailStruct) {
	switch (tokenDetailStruct->stemmedWordBuffer[tokenDetailStruct->endStringOffset
			- 1]) {
	case 'a':
		if (endStr(tokenDetailStruct, "\07" "ational")) {
			callReplaceBufferWithStringoIfConsonant(tokenDetailStruct,
					"\03" "ate");
			break;
		}
		if (endStr(tokenDetailStruct, "\06" "tional")) {
			callReplaceBufferWithStringoIfConsonant(tokenDetailStruct,
					"\04" "tion");
			break;
		}
		break;
	case 'c':
		if (endStr(tokenDetailStruct, "\04" "enci")) {
			callReplaceBufferWithStringoIfConsonant(tokenDetailStruct,
					"\04" "ence");
			break;
		}
		if (endStr(tokenDetailStruct, "\04" "anci")) {
			callReplaceBufferWithStringoIfConsonant(tokenDetailStruct,
					"\04" "ance");
			break;
		}
		break;
	case 'e':
		if (endStr(tokenDetailStruct, "\04" "izer")) {
			callReplaceBufferWithStringoIfConsonant(tokenDetailStruct,
					"\03" "ize");
			break;
		}
		break;
	case 'l':
		if (endStr(tokenDetailStruct, "\03" "bli")) {
			callReplaceBufferWithStringoIfConsonant(tokenDetailStruct,
					"\03" "ble");
			break;
		} /*-DEPARTURE-*/

		/* To match the published algorithm, replace this line with
		 case 'l': if (ends(z, "\04" "abli")) { r(z, "\04" "able"); break; } */

		if (endStr(tokenDetailStruct, "\04" "alli")) {
			callReplaceBufferWithStringoIfConsonant(tokenDetailStruct,
					"\02" "al");
			break;
		}
		if (endStr(tokenDetailStruct, "\05" "entli")) {
			callReplaceBufferWithStringoIfConsonant(tokenDetailStruct,
					"\03" "ent");
			break;
		}
		if (endStr(tokenDetailStruct, "\03" "eli")) {
			callReplaceBufferWithStringoIfConsonant(tokenDetailStruct,
					"\01" "e");
			break;
		}
		if (endStr(tokenDetailStruct, "\05" "ousli")) {
			callReplaceBufferWithStringoIfConsonant(tokenDetailStruct,
					"\03" "ous");
			break;
		}
		break;
	case 'o':
		if (endStr(tokenDetailStruct, "\07" "ization")) {
			callReplaceBufferWithStringoIfConsonant(tokenDetailStruct,
					"\03" "ize");
			break;
		}
		if (endStr(tokenDetailStruct, "\05" "ation")) {
			callReplaceBufferWithStringoIfConsonant(tokenDetailStruct,
					"\03" "ate");
			break;
		}
		if (endStr(tokenDetailStruct, "\04" "ator")) {
			callReplaceBufferWithStringoIfConsonant(tokenDetailStruct,
					"\03" "ate");
			break;
		}
		break;
	case 's':
		if (endStr(tokenDetailStruct, "\05" "alism")) {
			callReplaceBufferWithStringoIfConsonant(tokenDetailStruct,
					"\02" "al");
			break;
		}
		if (endStr(tokenDetailStruct, "\07" "iveness")) {
			callReplaceBufferWithStringoIfConsonant(tokenDetailStruct,
					"\03" "ive");
			break;
		}
		if (endStr(tokenDetailStruct, "\07" "fulness")) {
			callReplaceBufferWithStringoIfConsonant(tokenDetailStruct,
					"\03" "ful");
			break;
		}
		if (endStr(tokenDetailStruct, "\07" "ousness")) {
			callReplaceBufferWithStringoIfConsonant(tokenDetailStruct,
					"\03" "ous");
			break;
		}
		break;
	case 't':
		if (endStr(tokenDetailStruct, "\05" "aliti")) {
			callReplaceBufferWithStringoIfConsonant(tokenDetailStruct,
					"\02" "al");
			break;
		}
		if (endStr(tokenDetailStruct, "\05" "iviti")) {
			callReplaceBufferWithStringoIfConsonant(tokenDetailStruct,
					"\03" "ive");
			break;
		}
		if (endStr(tokenDetailStruct, "\06" "biliti")) {
			callReplaceBufferWithStringoIfConsonant(tokenDetailStruct,
					"\03" "ble");
			break;
		}
		break;
	case 'g':
		if (endStr(tokenDetailStruct, "\04" "logi")) {
			callReplaceBufferWithStringoIfConsonant(tokenDetailStruct,
					"\03" "log");
			break;
		} /*-DEPARTURE-*/
	}
}

// step3() deals with -ic-, -full, -ness etc. similar strategy to step2.
void StemmerFilterInternal::stepThree(struct TokenDetails * tokenDetailStruct) {
	switch (tokenDetailStruct->stemmedWordBuffer[tokenDetailStruct->endStringOffset]) {
	case 'e':
		if (endStr(tokenDetailStruct, "\05" "icate")) {
			callReplaceBufferWithStringoIfConsonant(tokenDetailStruct,
					"\02" "ic");
			break;
		}
		if (endStr(tokenDetailStruct, "\05" "ative")) {
			callReplaceBufferWithStringoIfConsonant(tokenDetailStruct,
					"\00" "");
			break;
		}
		if (endStr(tokenDetailStruct, "\05" "alize")) {
			callReplaceBufferWithStringoIfConsonant(tokenDetailStruct,
					"\02" "al");
			break;
		}
		break;
	case 'i':
		if (endStr(tokenDetailStruct, "\05" "iciti")) {
			callReplaceBufferWithStringoIfConsonant(tokenDetailStruct,
					"\02" "ic");
			break;
		}
		break;
	case 'l':
		if (endStr(tokenDetailStruct, "\04" "ical")) {
			callReplaceBufferWithStringoIfConsonant(tokenDetailStruct,
					"\02" "ic");
			break;
		}
		if (endStr(tokenDetailStruct, "\03" "ful")) {
			callReplaceBufferWithStringoIfConsonant(tokenDetailStruct,
					"\00" "");
			break;
		}
		break;
	case 's':
		if (endStr(tokenDetailStruct, "\04" "ness")) {
			callReplaceBufferWithStringoIfConsonant(tokenDetailStruct,
					"\00" "");
			break;
		}
		break;
	}
}

// step4() takes off -ant, -ence etc., in context <c>vcvc<v>.
void StemmerFilterInternal::stepFour(struct TokenDetails * tokenDetailStruct) {
	switch (tokenDetailStruct->stemmedWordBuffer[tokenDetailStruct->endStringOffset
			- 1]) {
	case 'a':
		if (endStr(tokenDetailStruct, "\02" "al"))
			break;
		return;

	case 'c':
		if (endStr(tokenDetailStruct, "\04" "ance"))
			break;
		if (endStr(tokenDetailStruct, "\04" "ence"))
			break;
		return;
	case 'e':
		if (endStr(tokenDetailStruct, "\02" "er"))
			break;
		return;
	case 'i':
		if (endStr(tokenDetailStruct, "\02" "ic"))
			break;
		return;
	case 'l':
		if (endStr(tokenDetailStruct, "\04" "able"))
			break;
		if (endStr(tokenDetailStruct, "\04" "ible"))
			break;
		return;
	case 'n':
		if (endStr(tokenDetailStruct, "\03" "ant"))
			break;
		if (endStr(tokenDetailStruct, "\05" "ement"))
			break;
		if (endStr(tokenDetailStruct, "\04" "ment"))
			break;
		if (endStr(tokenDetailStruct, "\03" "ent"))
			break;
		return;
	case 'o':
		if (endStr(tokenDetailStruct, "\03" "ion")
				&& (tokenDetailStruct->stemmedWordBuffer[tokenDetailStruct->stringOffset]
						== 's'
						|| tokenDetailStruct->stemmedWordBuffer[tokenDetailStruct->stringOffset]
								== 't'))
			break;
		if (endStr(tokenDetailStruct, "\02" "ou"))
			break;
		return;
		/* takes care of -ous */
	case 's':
		if (endStr(tokenDetailStruct, "\03" "ism"))
			break;
		return;
	case 't':
		if (endStr(tokenDetailStruct, "\03" "ate"))
			break;
		if (endStr(tokenDetailStruct, "\03" "iti"))
			break;
		return;
	case 'u':
		if (endStr(tokenDetailStruct, "\03" "ous"))
			break;
		return;
	case 'v':
		if (endStr(tokenDetailStruct, "\03" "ive"))
			break;
		return;
	case 'z':
		if (endStr(tokenDetailStruct, "\03" "ize"))
			break;
		return;
	default:
		return;
	}
	if (numberOfConsonantSequences(tokenDetailStruct) > 1)
		tokenDetailStruct->endStringOffset = tokenDetailStruct->stringOffset;
}

// removes a final -e if numberOfConsonantSequences() > 1, and changes -ll to -l if numberOfConsonantSequences() > 1.
void StemmerFilterInternal::stepFive(struct TokenDetails * tokenDetailStruct) {
	char * buff = tokenDetailStruct->stemmedWordBuffer;
	tokenDetailStruct->stringOffset = tokenDetailStruct->endStringOffset;
	if (buff[tokenDetailStruct->endStringOffset] == 'e') {
		int a = numberOfConsonantSequences(tokenDetailStruct);
		if ((a > 1 || a == 1)
				&& (!ConsonantVowelConsonantStructure(tokenDetailStruct,
						tokenDetailStruct->endStringOffset - 1)))
			tokenDetailStruct->endStringOffset--;
	}
	if (buff[tokenDetailStruct->endStringOffset] == 'l'
			&& doubleConsonant(tokenDetailStruct,
					tokenDetailStruct->endStringOffset)
			&& numberOfConsonantSequences(tokenDetailStruct) > 1)
		tokenDetailStruct->endStringOffset--;
}

// Does the stemming here and calls the functions related to step1 to t
std::string StemmerFilterInternal::stemUsingPorterRules(std::string token) {

	//	creating an instance of the TokenDetailsStruct and  allocating the struct size
	struct TokenDetails tokenDetailStruct;
	int endStringOffset = token.length() - 1;

	if (endStringOffset <= 1) {
		return token;
	}

	char *buff = new char[token.size() + 1];
	buff[token.size()] = 0;
	memcpy(buff, token.c_str(), token.size());

	tokenDetailStruct.stemmedWordBuffer = buff;
	tokenDetailStruct.endStringOffset = endStringOffset; /* copy the parameters into z */

	/* With this line, strings of length 1 or 2 don't go through the
	 stemming process, although no mention is made of this in the
	 published algorithm.
	 */
	stepOneAB(&tokenDetailStruct);
	stepOneC(&tokenDetailStruct);
	stepTwo(&tokenDetailStruct);
	stepThree(&tokenDetailStruct);
	stepFour(&tokenDetailStruct);
	stepFive(&tokenDetailStruct);

	std::string stemmedToken = token.substr(0, tokenDetailStruct.endStringOffset + 1);
	delete[] buff;
	return stemmedToken;
}

}
}
