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
#include "com_srch2_StringEngine.h"
#include "StringEngine.h"

/** Extracts the pointer to the c++ part of the StringEngine, handed down from
    the java part of the StringEngine by the given handle. The c++ part of the
    StringEngine is also informed of its JVM's current state. 
*/  
inline
StringEngine* dereferenceStringEngineHandle(JNIEnv *env, jlong handle) {
  StringEngine *rtn= (StringEngine*) handle;
  rtn->env= env;
  return rtn;
}

/* Initializes a new c++ StringEngine part with the instance language of its
   Java counterpart. The heap memory location of this new part is returned to
   its Java counterpart, so that it can be used as a handle to directed future
   bridged function calls.
*/
jlong Java_com_srch2_StringEngine_createStringEngine(JNIEnv *env,
    jclass, jobject refiningStringMethodgetValue,
    jclass searchableStringClassPtr, jobject searchableStringConstructor,
    jclass refiningStringClassPtr, jobject refiningStringConstructor,
    jclass indexedStringClassPtr, jobject indexedStringConstructor) {
 
  return 
    (jlong) new StringEngine(
       /* Extracts the constant JNI location of the given Method parameter. The
         parameter is a handle to a handle object which points to a given
         method; the extracted JNI location directly maps the method, relative
         to its associated class. This dramatically reduces lookup time; and, 
         the location remains constant, as long as its associated class
         persists, which is ensured by the above call. */
      env->FromReflectedMethod(refiningStringMethodgetValue),
      /* Creates a new permanent handle to the RefiningString Class. The new
         global reference creates a new handle object on the heap which
         references the same location as java handle referenced in the
         ClassPtr parameter; this call must be done in case the ClassPtr
         references a handle on the Java Stack, and thus will not persist for
         the next bridged function call. The persistent of this handle will
         also prevent Java from garbage collecting the RefiningString Class,
         even when no instances are present, since, a strong reference to it 
         will always be present.
       */
      (jclass) env->NewGlobalRef(searchableStringClassPtr), 
      env->FromReflectedMethod(searchableStringConstructor),
      (jclass) env->NewGlobalRef(refiningStringClassPtr), 
      env->FromReflectedMethod(refiningStringConstructor),
      (jclass) env->NewGlobalRef(indexedStringClassPtr), 
      env->FromReflectedMethod(indexedStringConstructor));
}

/** Stores the String Attribute's value passed down from the Java side of
    StringEngine, in the c++ side reference by the given handle */
void Java_com_srch2_StringEngine_setString
  (JNIEnv *env, jclass, jlong handle, jobject string) {
  dereferenceStringEngineHandle(env, handle)->setString(string);
}
/** Stores the given String Attributes's internalValue in this SearchableString
  */
void StringEngine::setString(jobject string) {
  /* it does not matter that refiningString is used for all String Attributes
     since they all share a common getValue method call */
  this->value = refiningString.toValue 
    <JNIClass::MakeUTF8StringFromUTF16JString>(string);
}

/** Returns the SearchableString equivalant of the string value stored in the
    c++ part of the StringEngine referenced by the given handle to the Java
    side of the StringEngine.
*/ 
jobject Java_com_srch2_StringEngine_getSearchableString (JNIEnv *env,
    jclass, jlong handle) {
  return dereferenceStringEngineHandle(env, handle)->getSearchableString();
}
/** Return a SearchableString with value equivalant to the one stored by this
    StringEngine.
  */
jobject StringEngine::getSearchableString() {
  return searchableString.createNew
    <JNIClass::MakeUTF8StringFromUTF16JString> (value);
}

/** Returns the RefiningString equivalent of the string value stored in the
    c++ part of the StringEngine referenced by the given handle to the Java
    side of the StringEngine.
*/ 
jobject Java_com_srch2_StringEngine_getRefiningString (JNIEnv *env,
    jclass, jlong handle) {
  return dereferenceStringEngineHandle(env, handle)->getRefiningString();
}

/** Return a RefiningString with value equivalent to the one stored by this
    StringEngine.
  */
jobject StringEngine::getRefiningString() {
  return refiningString.createNew
    <JNIClass::MakeUTF8StringFromUTF16JString> (value);
}

jobject Java_com_srch2_StringEngine_getIndexedString (JNIEnv *env,
    jclass, jlong handle) {
  return dereferenceStringEngineHandle(env, handle)->getIndexedString();
}
/** Return a IndexedString with value equivalent to the one stored by this
    StringEngine.
  */
jobject StringEngine::getIndexedString() {
  return indexedString.createNew
    <JNIClass::MakeUTF8StringFromUTF16JString> (value);
}

/** Deletes the c++ part of the StringEngine pointed to by the given handle */
void Java_com_srch2_StringEngine_deleteStringEngine(JNIEnv *env, jclass,
    jlong handle) {
  delete dereferenceStringEngineHandle(env, handle);
}

