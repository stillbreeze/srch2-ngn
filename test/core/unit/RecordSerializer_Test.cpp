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
#include "instantsearch/Schema.h"
#include "instantsearch/Constants.h"
#include "util/RecordSerializer.h"
#include <cstdio>
#include <cstring>
#include <cassert>

typedef SingleBufferAllocator Alloc;

using namespace srch2::util;

void compare(char const *expected, char const *actual, size_t num) {
  assert(!memcmp(expected, actual, num));
}

static char testSingleStringExpected[] = {
  0x8, 0x0, 0x0, 0x0,
  0xd, 0x0, 0x0, 0x0,
  'a', 'p', 'p', 'l', 'e' };

void testSingleString(Alloc &alloc) {
  srch2::instantsearch::Schema *schema = 
    srch2::instantsearch::Schema::create(srch2::instantsearch::DefaultIndex);

  int nameID = schema->setSearchableAttribute("name");

  RecordSerializer s(*schema);

  s.addSearchableAttribute(nameID, std::string("apple"));

  char *buffer = (char*) s.serialize().start;

  compare(testSingleStringExpected, buffer, 13);
}

static char testMultipleStringsExpected[] = {
  0xc, 0x0, 0x0, 0x0,
  0x11, 0x0, 0x0, 0x0,
  0x14, 0x0, 0x0, 0x0,
  'a', 'p', 'p', 'l', 'e',
  'c', ' ', '+' };

void testMultipleStrings(Alloc& alloc) {
  srch2::instantsearch::Schema *schema = 
    srch2::instantsearch::Schema::create(srch2::instantsearch::DefaultIndex);

  int nameID = schema->setSearchableAttribute("name");
  int addressID = schema->setSearchableAttribute("address");

  RecordSerializer s(*schema);

  s.addSearchableAttribute(nameID, std::string("apple"));
  s.addSearchableAttribute(addressID, std::string("c +"));

  char *buffer = (char*) s.serialize().start;;

  compare(testMultipleStringsExpected, buffer, 20);
}

static char testMultipleStringsOutofOrderExpected[] = {
  0xc, 0x0, 0x0, 0x0,
  0xf, 0x0, 0x0, 0x0,
  0x14, 0x0, 0x0, 0x0,
  'c', ' ', '+',
  'a', 'p', 'p', 'l', 'e'};

void testMultipleStringsOutOfOrder(Alloc& alloc) {
  srch2::instantsearch::Schema *schema = 
    srch2::instantsearch::Schema::create(srch2::instantsearch::DefaultIndex);

  int addressID = schema->setSearchableAttribute("address");
  int nameID = schema->setSearchableAttribute("name");

  RecordSerializer s(*schema);

  s.addSearchableAttribute(nameID, std::string("apple"));
  s.addSearchableAttribute(addressID, std::string("c +"));

  char *buffer = (char*) s.serialize().start;;

  compare(testMultipleStringsOutofOrderExpected, buffer, 20);
}

static char testIntAndStringExpected[] = {
  0x14, 0x0, 0x0, 0x0,
  0xc, 0x0, 0x0, 0x0,
  0x11, 0x0, 0x0, 0x0,
  'h', 'a', 'p', 'p', 'y'};

void testIntAndString(Alloc& alloc) {
  srch2::instantsearch::Schema *schema = 
    srch2::instantsearch::Schema::create(srch2::instantsearch::DefaultIndex);

  int nameID = schema->setSearchableAttribute("name");
  int addressID = 
    schema->setRefiningAttribute("address", 
        srch2::instantsearch::ATTRIBUTE_TYPE_INT, std::string("0"));

  RecordSerializer s(*schema);

  s.addRefiningAttribute(addressID, 20);
  s.addSearchableAttribute(nameID, std::string("happy"));

  char *buffer = (char*) s.serialize().start;;

  compare(testIntAndStringExpected, buffer, 20);
}

void testReuseBuffer(Alloc& alloc) {
  srch2::instantsearch::Schema *schema = 
    srch2::instantsearch::Schema::create(srch2::instantsearch::DefaultIndex);

  int nameID = schema->setSearchableAttribute("name");
  int addressID = 
    schema->setRefiningAttribute("address", 
        srch2::instantsearch::ATTRIBUTE_TYPE_INT, std::string("0"));

  RecordSerializer s(*schema);
  
  for(int i=0; i < 5; ++i) {
    s.nextRecord().addRefiningAttribute(addressID, 20);
    s.addSearchableAttribute(nameID, std::string("happy"));
    
    char *buffer = (char*) s.serialize().start;;
    compare(testIntAndStringExpected, buffer, 20);
  }
}

static char testIntAndStringOutOfOrderExpected[] = {
  0x3F, (char) 0xA7, (char) 0xdf, 0x5,
  0x0, 0x2, 0x0, 0x0,
  0x1c, 0x0, 0x0, 0x0,
  0x24, 0x0, 0x0, 0x0,
  0x29, 0x0, 0x0, 0x0,
  0x2d, 0x0, 0x0, 0x0,
  0x33, 0x0, 0x0, 0x0,
  'r', 'e', 'f', 'i', 'n', 'i', 'n', 'g',
  'a', 't', 't', 'i', 'c',
  'o', 'p', 'e', 'n', 
  'p', 'y', 't', 'h', 'o', 'n'};

void testIntAndStringOutOfOrder(Alloc& alloc) {
 srch2::instantsearch::Schema *schema = 
    srch2::instantsearch::Schema::create(srch2::instantsearch::DefaultIndex);

  int str1ID = schema->setSearchableAttribute("str1");
  int int1ID = 
    schema->setRefiningAttribute("int1", 
        srch2::instantsearch::ATTRIBUTE_TYPE_INT, std::string("0"));
  int str2ID = schema->setSearchableAttribute("str2");
  int int2ID = 
    schema->setRefiningAttribute("int2", 
        srch2::instantsearch::ATTRIBUTE_TYPE_INT, std::string("0"));
  int str3ID = schema->setSearchableAttribute("str3");
  int str4ID = 
    schema->setRefiningAttribute("int4", 
        srch2::instantsearch::ATTRIBUTE_TYPE_TEXT, std::string("0"));

  RecordSerializer s(*schema, alloc);
  
  for(int i=0; i < 5; ++i) {
    s.nextRecord().addSearchableAttribute(str3ID, std::string("python"));
    s.addRefiningAttribute(int2ID, 512);
    s.addSearchableAttribute(str1ID, std::string("attic"));
    s.addRefiningAttribute(int1ID, 98543423);
    s.addSearchableAttribute(str2ID, std::string("open"));
    s.addRefiningAttribute(str4ID, std::string("refining"));

    char *buffer = (char*) s.serialize().start;;
    compare(testIntAndStringOutOfOrderExpected, buffer, 39);
  }
}

static char testLongSerializationExpected[] = {
  0x2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
};

void testLongSerialization(Alloc& alloc) {
 srch2::instantsearch::Schema *schema = 
    srch2::instantsearch::Schema::create(srch2::instantsearch::DefaultIndex);

  int long1ID = 
    schema->setRefiningAttribute("long1", 
        srch2::instantsearch::ATTRIBUTE_TYPE_TIME, std::string("0"));

  RecordSerializer s(*schema);

  s.addRefiningAttribute(long1ID, (unsigned long) 2);

  char *buffer = (char*) s.serialize().start;;
  compare(testLongSerializationExpected, buffer, 8);
}

void testStringSerialization(Alloc& allocator) {
  testSingleString(allocator);
  testMultipleStrings(allocator);
  testMultipleStringsOutOfOrder(allocator);
  testIntAndString(allocator);
  testReuseBuffer(allocator);
  testIntAndStringOutOfOrder(allocator);
}


int main(int argc, char *argv[]) {
 /*   bool verbose = false;
    if (argc > 1 && strcmp(argv[1], "--verbose") == 0) {
        verbose = true;
    }*/
    Alloc alloc= Alloc();

    testStringSerialization(alloc);

    printf("RecordSerializer unit tests: Passed\n");

    return 0;
}
