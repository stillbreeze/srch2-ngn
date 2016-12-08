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
package com.srch2.android.sdk;

import android.util.Log;

abstract class IndexableCore {
    /**
     * If returned from {@link #getRecordCount()} indicates this value has not yet been set.
     * <br><br>
     * Has the <b>constant</b> value of {@code -1}.
     */
    public static final int INDEX_RECORD_COUNT_NOT_SET = -1;

    /**
     * The JSON key to use to retrieve each original record from each {@code JSONObject} in
     * the {@code ArrayList<JSONObject} of the callback method
     * {@link com.srch2.android.sdk.SearchResultsListener#onNewSearchResults(int, String, java.util.HashMap)}.
     * <br><br>
     * Has the <b>constant</b> value '{@code record}';
     */
    public static final String SEARCH_RESULT_JSON_KEY_RECORD = "record";

    /**
     * The JSON key to use to retrieve each set of highlighted fields from each record from each {@code JSONObject} in
     * the {@code ArrayList<JSONObject} of the callback method
     * {@link com.srch2.android.sdk.SearchResultsListener#onNewSearchResults(int, String, java.util.HashMap)}.
     * <br><br>
     * Has the <b>constant</b> value '{@code highlighted}';
     */
    public static final String SEARCH_RESULT_JSON_KEY_HIGHLIGHTED = "highlighted";

    /**
     * The default value for the numbers of search results to return per search request.
     * <br><br>
     * Has the <b>constant</b> value of {@code 10}.
     */
    public static final int DEFAULT_NUMBER_OF_SEARCH_RESULTS_TO_RETURN_AKA_TOPK = 10;

    /**
     * The default value for the fuzziness similarity threshold. Approximately one character
     * per every three characters will be allowed to act as a wildcard during a search.
     * <br><br>
     * Has the <b>constant</b> value of {@code 0.65}.
     */
    public static final float DEFAULT_FUZZINESS_SIMILARITY_THRESHOLD = 0.65f;


    IndexInternal indexInternal;

    /**
     * Implementing this method sets the name of the index this {@code Indexable}
     * or {@code SQLiteIndexable} represents.
     * @return the name of the index this {@code Indexable} or {@code SQLiteIndexable} represents
     */
    abstract public String getIndexName();


    /**
     * Implementing this method sets the highlighter for this {@code Indexable} or
     * {@code SQLiteIndexable}. If this method is not overridden the default highlighter
     * will be returned which will make prefix and fuzzy matching text bold using HTML tagging.
     * See {@link com.srch2.android.sdk.Highlighter} and {@link com.srch2.android.sdk.Highlighter.SimpleHighlighter}
     * for more information.
     * @return the highlighter used to visually match the text of the search input to the text of each searchable field
     */
    public Highlighter getHighlighter() {
        return new Highlighter();
    }
    int numberOfDocumentsInTheIndex = INDEX_RECORD_COUNT_NOT_SET;
    /**
     * Returns the number of records that are currently in the index that this
     * {@code Indexable} or {@code SQLiteIndexable} represents.
     * @return the number of records in the index
     */
    public int getRecordCount() {
        return numberOfDocumentsInTheIndex;
    }

    final void setRecordCount(int recordCount) {
        numberOfDocumentsInTheIndex = recordCount;
    }

    /**
     * Override this method to set the number of search results to be returned per query or search task.
     * <br><br>
     * The default value of this method, if not overridden, is ten.
     * <br><br>
     * This method will cause an {@code IllegalArgumentException} to be thrown when calling
     * {@link SRCH2Engine#initialize()} and passing this
     * {@code Indexable} or {@code SQLiteIndexable} if the returned value
     *  is less than one.
     * @return the number of results to return per search
     */
    public int getTopK() {
        return DEFAULT_NUMBER_OF_SEARCH_RESULTS_TO_RETURN_AKA_TOPK;
    }

    /**
     * Set the default fuzziness similarity threshold. This will determine how many character
     * substitutions the original search input will match search results for: if set to 0.5,
     * the search performed will include results as if half of the characters of the original
     * search input were replaced by wild card characters.
     * <br><br>
     * <b>Note:</b> In the the formation of a {@link Query}, each {@code Term} can
     * have its own fuzziness similarity threshold value set by calling the method
     * {@link SearchableTerm#enableFuzzyMatching(float)}; by default it is disabled for terms.
     * <br><br>
     * The default value of this method, if not overridden, is 0.65.
     * <br><br>
     * This method will cause an {@code IllegalArgumentException} to be thrown when calling
     * {@link SRCH2Engine#initialize()} and passing this {@code Indexable} or
     * {@code SQLiteIndexable} if the
     * value returned is less than zero or greater than one.
     * @return the fuzziness similarity ratio to match against search keywords against
     */
    public float getFuzzinessSimilarityThreshold() { return DEFAULT_FUZZINESS_SIMILARITY_THRESHOLD; }

    /**
     * Does a basic search on the index that this {@code Indexable} pr
     * {@code SQLiteIndexable} represents. A basic
     * search means that all distinct keywords (delimited by white space) of the
     * {@code searchInput} are treated as fuzzy, and the last keyword will
     * be treated as fuzzy and prefix.
     * <br><br>
     * For more configurable searches, use the
     * {@link Query} class in conjunction with the {@link #advancedSearch(Query)}
     * method.
     * <br><br>
     * When the SRCH2 server is finished performing the search task, the method
     * {@link SearchResultsListener#onNewSearchResults(int, String, java.util.HashMap)}
     *  will be triggered. The
     * {@code HashMap resultMap} argument will contain the search results in the form of {@code 
     * JSONObject}s as they were originally inserted (and updated).
     * <br><br>
     * This method will do nothing if the search input is null, an empty string or
     * {@link com.srch2.android.sdk.SRCH2Engine#onResume(android.content.Context)} has not yet
     * been called.
     * @param searchInput the textual input to search on
     */
    public final void search(String searchInput) {
        if (indexInternal != null) {
            indexInternal.search(searchInput);
        }
    }

    /**
     * Does an advanced search by forming search request input as a {@link Query}. This enables
     * searches to use all the advanced features of the SRCH2 search engine, such as per term
     * fuzziness similarity thresholds, limiting the range of results to a refining field, and
     * much more. See the {@link Query} for more details.
     * <br><br>
     * When the SRCH2 server is finished performing the search task, the method
     * {@link SearchResultsListener#onNewSearchResults(int, String, java.util.HashMap)}
     * will be triggered. The argument
     * {@code HashMap resultMap} will contain the search results in the form of {@code 
     * JSONObject}s as they were originally inserted (and updated).
     * <br><br>
     * This method will do nothing if the query is null or
     * {@link com.srch2.android.sdk.SRCH2Engine#onResume(android.content.Context)} has not yet
     * been called.
     * @param query the formation of the query to do the advanced search
     */
    public final void advancedSearch(Query query) {
        if (indexInternal != null) {
            indexInternal.advancedSearch(query);
        }
    }

    /**
     * Callback executed very shortly after the call to
     * {@link com.srch2.android.sdk.SRCH2Engine#onResume(android.content.Context)} is made:
     * when the SRCH2 search server is initialized by the {@code SRCH2Engine} (by the method just
     * mentioned), it will load each index into memory; this can take anywhere from a couple of milliseconds
     * to three seconds (depending on the number of records, how much data each record contains, and the
     * processing power of the device). When the index this {@code Indexable} or {@code SQLiteIndexable}}
     * represents is finished loading,
     * this method is thus triggered. At this point, all state operations on this index if it is
     * an {@code Indexable} are valid: insert, update, et
     * cetera; for both {@code Indexable} and {@code SQLiteIndexable} instances searches are now ready
     * at this point.
     * <br><br>
     * By overriding this method, its implementation can be used to verify the integrity of the index such as if
     * records need to be inserted (by checking {@link #getRecordCount()} for the first time) or likewise if the index
     * needs to be updated.
     * <br><br>
     * <i>This method does not have to be overridden</i> (thought it is <b>strongly encouraged</b> to do so). If it is
     * not, the number of records it contains upon being loaded will be printed to logcat
     * under the tag 'SRCH2' with the message prefixed by the name of the index this {@code Indexable} or
     * {@code SQLiteIndexable} represents.
     */
    public void onIndexReady() {
        Log.d("SRCH2", "Index " + getIndexName() + " is ready to be accessed and contains " + getRecordCount() + " records");
    }
}
