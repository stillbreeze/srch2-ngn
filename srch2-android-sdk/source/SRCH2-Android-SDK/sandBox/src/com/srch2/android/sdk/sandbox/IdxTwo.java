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
package com.srch2.android.sdk.sandbox;

import android.util.Log;
import com.srch2.android.sdk.*;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;

public class IdxTwo extends Indexable {

    public static final String INDEX_NAME = "nametwo";
    public static final String INDEX_FIELD_NAME_PRIMARY_KEY = "id";
    public static final String INDEX_FIELD_NAME_TITLE = "title";
    public static final String INDEX_FIELD_NAME_TITLE2 = "title2";
    public static final String INDEX_FIELD_NAME_SCORE = "score";


    @Override
    public String getIndexName() {
        return INDEX_NAME;
    }

    @Override
    public Schema getSchema() {
        PrimaryKeyField pk = Field.createDefaultPrimaryKeyField(INDEX_FIELD_NAME_PRIMARY_KEY);
        Field title = Field.createSearchableField(INDEX_FIELD_NAME_TITLE).enableHighlighting();
        Field title2 = Field.createSearchableField(INDEX_FIELD_NAME_TITLE2).enableHighlighting();
        RecordBoostField score = Field.createRecordBoostField(INDEX_FIELD_NAME_SCORE);
        return Schema.createSchema(pk, score, title, title2);
    }

    @Override
    public Highlighter getHighlighter() {
        return Highlighter.createHighlighter()
                .formatExactTextMatches(true, false, "#FF0000")
                .formatFuzzyTextMatches(true, false, "#FF00FF");
    }

    final public JSONArray getRecords() {
        JSONArray records = new JSONArray();
        try {
            JSONObject o1 = new JSONObject();
            o1.put(INDEX_FIELD_NAME_PRIMARY_KEY, "1");
            o1.put(INDEX_FIELD_NAME_TITLE, "Title Apple One");
            o1.put(INDEX_FIELD_NAME_TITLE2, "one apple");
            o1.put(INDEX_FIELD_NAME_SCORE, 10);
            records.put(o1);

            o1 = new JSONObject();
            o1.put(INDEX_FIELD_NAME_PRIMARY_KEY, "7");
            o1.put(INDEX_FIELD_NAME_TITLE, "Title Parrot Seven");
            o1.put(INDEX_FIELD_NAME_SCORE, 5);
            o1.put(INDEX_FIELD_NAME_TITLE2, "seven parrots");
            records.put(o1);

            o1 = new JSONObject();
            o1.put(INDEX_FIELD_NAME_PRIMARY_KEY, "6");
            o1.put(INDEX_FIELD_NAME_TITLE, "Title Penguin Six");
            o1.put(INDEX_FIELD_NAME_SCORE, 10);
            o1.put(INDEX_FIELD_NAME_TITLE2, "six penguins");
            records.put(o1);

            o1 = new JSONObject();
            o1.put(INDEX_FIELD_NAME_PRIMARY_KEY, "2");
            o1.put(INDEX_FIELD_NAME_TITLE, "Title Chicken Two");
            o1.put(INDEX_FIELD_NAME_SCORE, 90);
            o1.put(INDEX_FIELD_NAME_TITLE2, "two chickens");
            records.put(o1);

            o1 = new JSONObject();
            o1.put(INDEX_FIELD_NAME_PRIMARY_KEY, "3");
            o1.put(INDEX_FIELD_NAME_TITLE, "Title Turkey Three");
            o1.put(INDEX_FIELD_NAME_SCORE, 80);
            o1.put(INDEX_FIELD_NAME_TITLE2, "three turkeys");
            records.put(o1);

            o1 = new JSONObject();
            o1.put(INDEX_FIELD_NAME_PRIMARY_KEY, "4");
            o1.put(INDEX_FIELD_NAME_TITLE, "Title Parakeet Four");
            o1.put(INDEX_FIELD_NAME_SCORE, 65);
            o1.put(INDEX_FIELD_NAME_TITLE2, "four parakeets");
            records.put(o1);

            o1 = new JSONObject();
            o1.put(INDEX_FIELD_NAME_PRIMARY_KEY, "5");
            o1.put(INDEX_FIELD_NAME_TITLE, "Title Elephant Five");
            o1.put(INDEX_FIELD_NAME_SCORE, 25);
            o1.put(INDEX_FIELD_NAME_TITLE2, "five elephants");
            records.put(o1);

        } catch (JSONException ee) {
        }
        return records;
    }

    static public ArrayList<SearchResultsAdapter.SearchResultItem> wrap(ArrayList<JSONObject> jsonResultsToWrap) {
        ArrayList<SearchResultsAdapter.SearchResultItem> newResults = new ArrayList<SearchResultsAdapter.SearchResultItem>();
        for (JSONObject jsonObject : jsonResultsToWrap) {
            Log.d("SEARCH RESULT OBJECT", jsonObject.toString());
            SearchResultsAdapter.SearchResultItem searchResult = null;
            try {

                JSONObject originalRecord = jsonObject.getJSONObject(Indexable.SEARCH_RESULT_JSON_KEY_RECORD);

                JSONObject highlightRecord = jsonObject.getJSONObject(Indexable.SEARCH_RESULT_JSON_KEY_HIGHLIGHTED);

                String title = highlightRecord.getString("title");

                Log.d("Highlight", "title is " + title);

                if (title == null) {
                    title =  "null title";
                }

                searchResult = new SearchResultsAdapter.SearchResultItem(title, " ", " ");
            } catch (JSONException oops) {
                oops.printStackTrace();
                continue;
            }

            if (searchResult != null) {
                newResults.add(searchResult);
            }
        }
        return newResults;
    }

    @Override
    public void onIndexReady() {
        super.onIndexReady();

    }

    @Override
    public void onInsertComplete(int success, int failed, String JSONResponse) {
        super.onInsertComplete(success, failed, JSONResponse);
    }
}
