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

import android.os.Bundle;
import android.util.Log;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;

import static junit.framework.Assert.*;

public class CrudTestActivity extends TestableActivity {
    public static final String TAG = "srch2:: MyActivity";

    public TestSearchResultsListener mResultListener = new TestSearchResultsListener();

    public TestableIndex mIndex1 = new TestIndex();
    public TestableIndex mIndex2 = new TestIndex2();
    public TestGeoIndex mIndexGeo = new TestGeoIndex();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setTitle(TAG);

        Log.d(TAG, "onCreate");
    }

    @Override
    protected void onResume() {
        super.onResume();
        Log.d(TAG, "onResume");
    }

    @Override
    protected void onPause() {
        super.onPause();
        Log.d(TAG, "onPause");
    }

    @Override
    protected void onStart() {
        super.onStart();
        Log.d(TAG, "onResume");
    }

    @Override
    protected void onStop() {
        super.onStop();
        Log.d(TAG, "onPause");
    }

    @Override
    protected void onRestart() {
        super.onRestart();
        Log.d(TAG, "onRestart");
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        Log.d(TAG, "onDestroy");
    }

    public void initializeSRCH2EngineAndCallStart() {
        initializeSRCH2Engine();
        try {
            Thread.currentThread().sleep(300);
        } catch (InterruptedException e) {

        }
        callSRCH2EngineStart();
    }

    public void initializeSRCH2Engine() {
        SRCH2Service.clearServerLogEntriesForTest(getApplicationContext());
        deleteSrch2Files();
        SRCH2Engine.setIndexables(mIndex1, mIndex2, mIndexGeo);
        SRCH2Engine.setSearchResultsListener(mResultListener);
        SRCH2Engine.setAutomatedTestingMode(true);
        SRCH2Engine.onResume(getApplicationContext());
    }

    public void callSRCH2EngineStart() {
        SRCH2Engine.onResume(this);
    }

    public void callSRCH2EngineStop() {
        SRCH2Engine.onPause(this);
    }

    private void reset() {
        mResultListener.reset();
    }

    public String getInsertResponse(TestableIndex index) {
        Util.waitForResponse(Util.ResponseType.Insert, index);
        return index.insertResponse;
    }

    public String getDeleteResponse(TestableIndex index) {
        Util.waitForResponse(Util.ResponseType.Delete, index);
        return index.deleteResponse;
    }

    public String getUpdateResponse(TestableIndex index) {
        Util.waitForResponse(Util.ResponseType.Update, index);
        return index.updateResponse;
    }

    public String getRecordResponse(TestableIndex index) {
        Util.waitForResponse(Util.ResponseType.GetRecord, index);
        return index.getRecordResponse;
    }

    public SearchResultsListener getSearchResult() {
        Util.waitForResultResponse(mResultListener);
        return mResultListener;
    }

    public void waitForEngineReady() {
        Util.waitSRCH2EngineIsReady();
    }

    public void testAll() {
        try {
            for (TestableIndex index : new TestableIndex[]{ mIndex1, mIndexGeo}) {
                testOneRecordCRUD(index);
                testBatchRecordCRUD(index);
            }
        } catch (JSONException e) {
            e.printStackTrace();
            //Assert.fail();
        }
    }

    public void testStartEngine() {
        TestableIndex[] indexes = {mIndex1, mIndex2, mIndexGeo};
        for (TestableIndex index : indexes) {
            assertTrue(SRCH2Engine.getIndex(index.getIndexName()).getRecordCount() == Indexable.INDEX_RECORD_COUNT_NOT_SET);
        }

        Log.i(TAG, "testStartEngine");
        waitForEngineReady();
        for (TestableIndex index : indexes) {
            assertTrue(SRCH2Engine.getIndex(index.getIndexName()).getRecordCount() == 0);
            assertTrue(index.indexIsReadyCalled);
            index.indexIsReadyCalled = false;
        }
    }



    public void testOneRecordCRUD(TestableIndex index) throws JSONException {

        Log.i(TAG, "testIndexableWithNoRecordsHasZeroRecordCount");
        testIndexableGetRecordCountMatches(index, 0);

        Log.i(TAG, "testOneRecordCRUD");
        JSONObject record = index.getSucceedToInsertRecord();

        Log.i(TAG, "testInsertShouldSuccess");
        testInsertShouldSuccess(index, record);

        Log.i(TAG, "testIndexableWithOneRecordedInsertedHasOneRecordCount");
        testIndexableGetRecordCountMatches(index, 1);

        Log.i(TAG, "testInsertShouldFail");
        testInsertShouldFail(index, index.getFailToInsertRecord());

        Log.i(TAG, "testIndexableWithOneRecordedInsertedHasOneRecordCountAndOneFailedInsert");
        testIndexableGetRecordCountMatches(index, 1);

        Log.i(TAG, "testGetRecordIdShouldSuccess");
        testGetRecordIdShouldSuccess(index, new JSONArray(Arrays.asList(record)));

        Log.i(TAG, "testSearchStringShouldSuccess");
        testSearchStringShouldSuccess(index, index.getSucceedToSearchString(new JSONArray(Arrays.asList(record))));

        Log.i(TAG, "testSearchStringShouldFail");
        testSearchStringShouldFail(index, index.getFailToSearchString(new JSONArray(Arrays.asList(record))));

        Log.i(TAG, "testSearchQueryShouldSuccess");
        testSearchQueryShouldSuccess(index, index.getSucceedToSearchQuery(new JSONArray(Arrays.asList(record))));

        Log.i(TAG, "testSearchQueryShouldFail");
        testSearchQueryShouldFail(index, index.getFailToSearchQuery(new JSONArray(Arrays.asList(record))));

        Log.i(TAG, "testUpdateExistShouldSuccess");
        testUpdateExistShouldSuccess(index, index.getSucceedToUpdateExistRecord());

        Log.i(TAG, "testUpdateNewShouldSuccess");
        testUpdateNewShouldSuccess(index, index.getSucceedToUpsertRecord());

        Log.i(TAG, "testIndexableWithOneRecordedInsertedAndOneRecordUpsertedGetRecordCount");
        testIndexableGetRecordCountMatches(index, 2);

        Log.i(TAG, "testUpdateShouldFail");
//        TODO too much problem inside the http error responds inside th engine, need time to clean up
//        testUpdateShouldFail(index, index.getFailToUpdateRecord());

        Log.i(TAG, "testDeleteShouldSuccess");
        testDeleteShouldSuccess(index, Arrays.asList(record.getString(index.getPrimaryKeyFieldName())));

        Log.i(TAG, "testIndexableWithTwoRecordsAddedThenOneDeletedGetRecordCount");
        testIndexableGetRecordCountMatches(index, 1);

        testDeleteShouldSuccess(index, Arrays.asList(index.getSucceedToUpsertRecord().getString(index.getPrimaryKeyFieldName())));

      Log.i(TAG, "testIndexableWithTwoRecordsAddedThenBothDeleted");
        testIndexableGetRecordCountMatches(index, 0);

        Log.i(TAG, "testDeleteShouldFail");
       testDeleteShouldFail(index, index.getFailToDeleteRecord());
    }


    public void testBatchRecordCRUD(TestableIndex index) throws JSONException {
        JSONArray records = index.getSucceedToInsertBatchRecords();

        Log.i(TAG, "testIndexableGetRecordBeforeBatchInsert");
//        testIndexableGetRecordCountMatches(index, 0);

        Log.i(TAG, "testBatchInsertShouldSuccess");
        testBatchInsertShouldSuccess(index, records);

        Log.i(TAG, "testIndexableWith200BatchInsertsGetRecordShouldMatch");
        testIndexableGetRecordCountMatches(index, records.length());

        Log.i(TAG, "testGetRecordIdShouldSuccess");
        testGetRecordIdShouldSuccess(index, records);

        Log.i(TAG, "testBatchInsertShouldFail");
        testBatchInsertShouldFail(index, index.getFailToInsertBatchRecord());

        Log.i(TAG, "testSearchStringShouldSuccess");
        testSearchStringShouldSuccess(index, index.getSucceedToSearchString(records));

        Log.i(TAG, "testSearchStringShouldFail");
        testSearchStringShouldFail(index, index.getFailToSearchString(records));

        Log.i(TAG, "testSearchQueryShouldSuccess");
        testSearchQueryShouldSuccess(index, index.getSucceedToSearchQuery(records));

        Log.i(TAG, "testSearchQueryShouldFail");
        testSearchQueryShouldFail(index, index.getFailToSearchQuery(records));

        Log.i(TAG, "testBatchUpdateShouldSuccess");
        testBatchUpdateShouldSuccess(index, index.getSucceedToUpdateBatchRecords());

        Log.i(TAG, "testBatchUpdateShouldFail");
        //TODO recover this test after fix the engine response
        //testBatchUpdateShouldFail(index, index.getFailToUpdateBatchRecords());

        ArrayList<String> ids = new ArrayList<String>();
        for (int i = 0; i < records.length(); ++i) {
            ids.add(records.getJSONObject(i).getString(index.getPrimaryKeyFieldName()));
        }

        Log.i(TAG, "testDeleteShouldSuccess");
        testDeleteShouldSuccess(index, ids);

        Log.i(TAG, "testIndexableWithAllRecordsDeleted");
        testIndexableGetRecordCountMatches(index, 0);

        Log.i(TAG, "testDeleteShouldFail");
        testDeleteShouldFail(index, ids);

    }

    private void testGetRecordIdShouldSuccess(TestableIndex index, JSONArray records) throws JSONException {
        for (int i = 0; i < records.length(); i++) {
            index.getRecordbyID(records.getJSONObject(i).getString(index.getPrimaryKeyFieldName()));
            getRecordResponse(index);
//            Log.i(TAG, "expected record::tostring():" + records.getJSONObject(i).toString());
//            Log.i(TAG, "actual response::tostring():" + mControlListener.recordResponse.record.toString());
            // TODO wait engine to fix the all string type record
            //assertTrue(mControlListener.recordResponse.record.toString().equals(records.getJSONObject(i).toString()));
            JSONObject resultRecord = index.recordRetreived;
            JSONObject record = resultRecord.getJSONObject(Indexable.SEARCH_RESULT_JSON_KEY_RECORD);
            assertTrue(record.getString(
                    index.getPrimaryKeyFieldName()).equals(records.getJSONObject(i).getString(index.getPrimaryKeyFieldName())));
            index.resetGetRecordResponseFields();
        }
    }

    public void testMultiCoreSearch() {
        // simplify the test cases, the mIndex1 and mIndex2 are of the same
        TestableIndex [] testIndexes= {mIndex1, mIndex2, mIndexGeo};
        JSONArray records = mIndex1.getSucceedToInsertBatchRecords();

        Log.d(TAG, records.toString());
        for(TestableIndex index : testIndexes) {

            Log.i(TAG, "testBatchInsertShouldSuccess");
            testBatchInsertShouldSuccess(index, records);
            try {
                Thread.currentThread().sleep(1000);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }


        for (String query : mIndex1.getSucceedToSearchString(records)) {
            mResultListener.reset();
            SRCH2Engine.searchAllIndexes(query);
            getSearchResult();

            assertTrue(mResultListener.resultRecordMap.size() == testIndexes.length);
            for(TestableIndex index : testIndexes) {
                assertTrue(index.verifyResult(query, mResultListener.resultRecordMap.get(index.getIndexName())));
            }
        }

        for (Query query : mIndex1.getSucceedToSearchQuery(records)) {
            mResultListener.reset();
            SRCH2Engine.advancedSearchOnAllIndexes(query);
            getSearchResult();
            assertTrue(mResultListener.resultRecordMap.size() == testIndexes.length);
            for(TestableIndex index : testIndexes) {
                assertTrue(index.verifyResult(query, mResultListener.resultRecordMap.get(index.getIndexName())));
            }
        }

    }

    public void testIndexableGetRecordCountMatches(TestableIndex index, int expectedNumberOfRecords) {
        assertEquals(index.getRecordCount(), expectedNumberOfRecords);
    }

    public void testInsertShouldSuccess(TestableIndex index, JSONObject record) {
        index.resetInsertResponseFields();
        index.insert(record);
        getInsertResponse(index);
        assertTrue(index.insertSuccessCount == 1);
    }

    public void testInsertShouldFail(TestableIndex index, JSONObject record) {
        index.resetInsertResponseFields();
        index.insert(record);
        getInsertResponse(index);
        assertTrue(index.insertSuccessCount == 0);
        assertTrue(index.insertFailedCount == 1);
    }

    public void testBatchInsertShouldSuccess(TestableIndex index, JSONArray array) {
        index.resetInsertResponseFields();
        index.insert(array);
        getInsertResponse(index);
        assertTrue(index.insertSuccessCount == array.length());
        assertTrue(index.insertFailedCount == 0);
    }

    public void testBatchInsertShouldFail(TestableIndex index, JSONArray array) {
        index.resetInsertResponseFields();
        index.insert(array);
        getInsertResponse(index);
        assertTrue(index.insertSuccessCount == 0);
        assertTrue(index.insertFailedCount == array.length());
    }

    public void testSearchStringShouldSuccess(TestableIndex index, List<String> queries) {
        for (String query : queries) {
            mResultListener.reset();
            index.search(query);
            getSearchResult();
            HashMap<String, ArrayList<JSONObject>> recordMap = mResultListener.resultRecordMap;
            assertTrue(recordMap.size() == 1);
            ArrayList<JSONObject> records = recordMap.get(index.getIndexName());
            assertNotNull(records);
            assertTrue(index.verifyResult(query, records));
        }
    }

    public void testSearchStringShouldFail(TestableIndex index, List<String> queries) {
        for (String query : queries) {
            mResultListener.reset();
            index.search(query);
            getSearchResult();
            assertTrue(mResultListener.resultRecordMap.size() == 1);
            assertTrue(mResultListener.resultRecordMap.get(index.getIndexName()).size() == 0);
        }
    }

    public void testSearchQueryShouldSuccess(TestableIndex index, List<Query> queries) {
        for (Query query : queries) {
            mResultListener.reset();
            index.advancedSearch(query);
            getSearchResult();
            assertTrue(mResultListener.resultRecordMap.size() == 1);
            assertTrue(mResultListener.resultRecordMap.get(index.getIndexName()) != null);
            assertTrue(index.verifyResult(query, mResultListener.resultRecordMap.get(index.getIndexName())));
        }
    }

    public void testSearchQueryShouldFail(TestableIndex index, List<Query> queries) {
        for (Query query : queries) {
            mResultListener.reset();
            index.advancedSearch(query);
            getSearchResult();
            assertTrue(mResultListener.resultRecordMap.size() == 1);
            Cat.d("testSearchQueryShouldFail::Query:", query.toString());
            assertTrue(mResultListener.resultRecordMap.get(index.getIndexName()).size() == 0);
        }
    }

    public void testUpdateExistShouldSuccess(TestableIndex index, JSONObject record) {
        index.resetUpdateResponseFields();
        index.update(record);
        getUpdateResponse(index);
        Cat.d("testUpdateExistShouldSuccess:", index.updateResponse);
        assertTrue(index.updateSuccessCount == 1);
        assertTrue(index.upsertSuccessCount == 0);
        assertTrue(index.updateFailedCount == 0);
    }

    public void testUpdateNewShouldSuccess(TestableIndex index, JSONObject record) {
        index.resetUpdateResponseFields();
        index.update(record);
        getUpdateResponse(index);
        assertTrue(index.updateSuccessCount == 0);
        assertTrue(index.upsertSuccessCount == 1);
        assertTrue(index.updateFailedCount == 0);
    }

    public void testUpdateShouldFail(TestableIndex index, JSONObject record) {
        index.resetUpdateResponseFields();
        index.update(record);
        getUpdateResponse(index);
        assertTrue(index.updateSuccessCount == 0);
        assertTrue(index.updateFailedCount == 1);
    }

    public void testBatchUpdateShouldSuccess(TestableIndex index, JSONArray array) {
        index.resetUpdateResponseFields();
        index.update(array);
        getUpdateResponse(index);
        assertTrue(index.updateSuccessCount == array.length());
        assertTrue(index.updateFailedCount == 0);
    }

    public void testBatchUpdateShouldFail(TestableIndex index, JSONArray array) {
        index.resetUpdateResponseFields();
        index.update(array);
        getUpdateResponse(index);
        assertTrue(index.updateSuccessCount == 0);
        assertTrue(index.updateFailedCount == array.length());
    }

    public void testDeleteShouldSuccess(TestableIndex index, List<String> ids) {
        for (String id : ids) {
            index.resetDeleteResponseFields();
            index.delete(id);
            getDeleteResponse(index);
            assertTrue(index.deleteSuccessCount == 1);
            assertTrue(index.deleteFailedCount == 0);
        }
    }

    public void testDeleteShouldFail(TestableIndex index, List<String> ids) {
        for (String id : ids) {
            index.resetDeleteResponseFields();
            index.delete(id);
            getDeleteResponse(index);
            assertTrue(index.deleteSuccessCount == 0);
            assertTrue(index.deleteFailedCount == 1);
        }
    }

    @Override
    public List<String> getTestMethodNameListWithOrder() {
        return Arrays.asList(new String[]{
                "testStartEngine"
                ,"testAll"
//                ,"testMultiCoreSearch"
        });
    }

    @Override
    public void beforeAll() {
        initializeSRCH2EngineAndCallStart();
    }

    @Override
    public void afterAll() {
        callSRCH2EngineStop();
    }

    @Override
    public void beforeEach() {

    }

    @Override
    public void afterEach() {

    }
}

