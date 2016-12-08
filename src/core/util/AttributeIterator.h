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

#ifndef __ATTRIBUTE_ITERATOR_H__
#define  __ATTRIBUTE_ITERATOR_H__

#include <iterator>

namespace srch2 {
namespace instantsearch {

/* Iterators over all the set Attributes in a given attribute mask, ie. if the
   attribute mask 0000..01001001 was given the iterator would return
   0 then 4 then 7, and then subsequent calls to hasNext will return 0 */
class AttributeIterator : std::iterator<unsigned, std::forward_iterator_tag> {
  unsigned char bit;
  unsigned place;
  unsigned mask;

  public:
  AttributeIterator(unsigned mask) : bit(__builtin_ffs(mask)), place(bit-1),
    mask(mask) {}

  AttributeIterator& operator++() {
    mask= mask >> bit;
    bit= __builtin_ffs(mask);
    place+= bit;
    
    return *this;
  }

  unsigned operator*() { return place;}

  bool hasNext() { return mask; }
};

}}

#endif /* __ATTRIBUTE_ITERATOR_H__ */
