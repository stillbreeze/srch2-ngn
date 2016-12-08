
/*****************************************************************************
 *                                                                           *
 *              AUTHOR : RJ ATWAL                                            *
 *                                                                           *
 *                                                                           * 
  ****************************************************************************/

package com.srch2;

import java.lang.reflect.Method;
import java.lang.reflect.Constructor;

/** Mock up of the Srch2 Engine to test a particular facet of the sdk */
class StringEngine {
  /** Performed each time Java Loads this Class */
  static {
    /** Dynamically links in libsrch2_string_core.so to bind native methods */
    java.lang.System.loadLibrary("string_engine");
  }

  /* A handle to the c++ part of this StringEngine */
  private long handle;

  StringEngine() throws NoSuchMethodException {
     /* Java handle to the RefiningString method getValue, invoking this
        handle with a RefiningString instance, namely,

           getString.invoke(searchableStringInstance) 

             is equivalent to 
           
           searchableStringInstance.getValue();
     */
     Method getString = Attribute.class.getMethod("getValue");
     /* Java handle to the RefiningString constructor, invoking this
        handle with a UTF16 encoded string, namely

        makeRefiningString.newInstance(String);

          is equivalent to

        RefiningString(String);
     */
     Constructor makeRefiningString=
       RefiningString.class.getConstructor(String.class);
     /* Java handle to the SearchableString constructor, invoking this
        handle with a UTF16 encoded string, namely

        makeSearchableString.newInstance(String);

          is equivalent to

        SearchableString(String);
     */

     Constructor makeSearchableString=
       SearchableString.class.getConstructor(String.class);
     /* Java handle to the IndexedString constructor, invoking this
        handle with a UTF16 encoded string, namely

        makeIndexedString.newInstance(String);

          is equivalent to

        IndexedString(String);
     */
     Constructor makeIndexedString=
       IndexedString.class.getConstructor(String.class);


     handle = createStringEngine(getString, SearchableString.class, 
         makeSearchableString, RefiningString.class,
         makeRefiningString, IndexedString.class, makeIndexedString);
 }

  /** Sets up the c++ part of this StringEngine, informing it of language 
      used by the Java part, namely this instance, of this StringEngine. 
      The Language used by this engine surrounds the RefiningString Class,
      which is instanced as an Object basing all its instances; this is the 
      first parameter passed. This class contains two functional members to 
      manipulate its instances: one, which the second parameter refences a
      handle of, is a method returning a constant reference to its underlying
      UFT16 encoded string value; the second, which bases the handle referenced
      by the third parameter, is a constructor of a RefiningString object
      which takes a single argument, namely its UTF16 String value
  */
  native static long
    createStringEngine(Method getString, 
        Class<SearchableString> searchableStringClass,
        Constructor createSearchableString,
        Class<RefiningString> refiningStringClass,
        Constructor createRefiningString,
        Class<IndexedString> indexedStringClass,
        Constructor createIndexedString);

  /** Passing a String Attribute to the StringEngine. This is used in testing
      the JNI handling of String Attributes */
  public void setString(Attribute<String> string) {
    setString(handle, string);
  }
  /** Passes the String Attribute to the C++ part of this Engine, reference by
      the passed handle, for storage */
  private static native void setString(long handle, Attribute<String> string);

/** Returns a SearchableString from the StringEngine. This is used in
      testing the JNI handling of SearchableStrings */
  public SearchableString getSearchableString() {
    return getSearchableString(handle);
  }
  /** Returns a RefiningString from the StringEngine. This is used in
      testing the JNI handling of RefiningStrings */
  public RefiningString getRefiningString() {
    return getRefiningString(handle);
  }
  /** Returns a IndexedString from the StringEngine. This is used in
      testing the JNI handling of IndexedStrings */
  public IndexedString getIndexedString() {
    return getIndexedString(handle);
  }

  /** Returns the SearchableString stored in the C++ part of this Engine,
      reference by the passed handle */
  private static native SearchableString getSearchableString(long handle);
  /** Returns the RefiningString stored in the C++ part of this Engine,
      reference by the passed handle */
  private static native RefiningString getRefiningString(long handle);
  /** Returns the IndexedString stored in the C++ part of this Engine,
      reference by the passed handle */
  private static native IndexedString getIndexedString(long handle);

  /** Free the heap memory storing the c++ side of this StringEngine */
  private native void deleteStringEngine(long handle);
}

