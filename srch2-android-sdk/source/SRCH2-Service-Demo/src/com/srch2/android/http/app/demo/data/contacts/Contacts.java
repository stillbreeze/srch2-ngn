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
package com.srch2.android.http.app.demo.data.contacts;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.HashMap;

import org.json.JSONArray;

import android.content.Context;
import android.provider.ContactsContract;
import android.util.Log;

import com.srch2.android.http.app.demo.data.SourceDataRecords;
import com.srch2.android.http.app.demo.data.incremental.IncrementalState;
import com.srch2.android.http.app.demo.data.incremental.IncrementalStateFunctor;
import com.srch2.android.http.app.demo.data.incremental.IncrementalValuePair;
import com.srch2.android.http.service.Index;


public class Contacts {
	
	public static final String TAG = "ContactsIndexWrapper";
	
	public Index index;
	public IncrementalState incrementalState;
	IndexUpdater updater;
	
	public Contacts(Index theIndex, Context c) {
		Log.d("srch2:: " + TAG, "Contacts()");
		index = theIndex;
		updater = new IndexUpdater(c);
		incrementalState = new IncrementalState(ContactsContract.RawContacts.CONTENT_URI, updater);
	}
	
	public void startObserveringIncrementalState(Context context) {
		Log.d("srch2:: " + TAG, "startObserveringIncrementalState");
		incrementalState.registerObserver(context);
	}

	public void stopObserveringIncrementalState(Context context) {
		Log.d("srch2:: " + TAG, "stopObserveringIncrementalState");
		incrementalState.unregisterObserver(context);
	}
	
	public void setDeserializedSnapshot(ArrayList<IncrementalValuePair> snapshot) {
		incrementalState.getAndSetLatestSnapShot(snapshot);
	}
	
	public void buildIndex(final Context context) {
		Log.d("srch2:: " + TAG, "buildIndex");
		Thread t = new Thread(new Runnable() {
			
			@Override
			public void run() {
				Log.d("srch2:: " + TAG, "buildIndex - run - start");
				SourceDataRecords records = IterateRawContacts.iterate(IterateRawContacts.getCursor(context.getContentResolver()));
				IterateRawContactEntities.iterate(IterateRawContactEntities.getCursor(context.getContentResolver()), records);
				
				for (IncrementalValuePair p : records.incrementalValuePairs) {
					incrementalState.addSingle(p);
				}
				
				Log.d("srch2:: " + TAG, "buildIndex - run size of records is " + records.size());
				JSONArray jarray = records.getJsonArray();
				// do srch2 insert
				index.insert(jarray); 
				Log.d("srch2:: " + TAG, "buildIndex - run called index.insert");
			
			}
		});
		t.start();
	}
	
	public void updateIndex(final Context context) {
		updater.updateIndex();
	}
	
	class IndexUpdater implements IncrementalStateFunctor {
		WeakReference<Context> contextReference;
		
		public IndexUpdater(Context c) {
			contextReference = new WeakReference<Context>(c);
		}
		
		@Override
		public void updateIndex() {
			Log.d("srch2:: " + TAG, "updateIndex");
			
			final Context c = contextReference.get();
			if (c != null) {
				Thread t = new Thread(new Runnable() {
					@Override
					public void run() {
						 Log.d("srch2:: " + TAG, "updateIndex - run - start");
						 HashMap<Boolean, SourceDataRecords> diffRecords = IterateContactsDifference.inspectForRecordsToUpdate(c.getContentResolver(), incrementalState.getSnapShotAsHashSet());
						 
						 SourceDataRecords deleteRecords = diffRecords.get(false);
						 
						
						 if (deleteRecords != null && deleteRecords.size() > 0) {
							 Log.d("srch2:: " + TAG, "updateIndex - run - deleteRecords size was " + deleteRecords.size());
							 int[] deleteIds = deleteRecords.getDeleteIds();
							 for (Integer i : deleteIds) {
								 index.delete(String.valueOf(i));
								 incrementalState.removeSingle(i);
							 }
						 } else {
							 Log.d("srch2:: " + TAG, "updateIndex - run - deleteRecords size was 0 or null");
						 }
						 
						 SourceDataRecords addRecords = diffRecords.get(true);
						 if (addRecords != null && addRecords.size() > 0) {
							Log.d("srch2:: " + TAG, "updateIndex - run - addRecords size was " + addRecords.size());
							index.insert(addRecords.getJsonArray());
							for (IncrementalValuePair ivp : addRecords.incrementalValuePairs) {
								incrementalState.addSingle(ivp);
							}
						 } else {
							 Log.d("srch2:: " + TAG, "updateIndex - run - addRecords size was 0 or null");
						 }
					}
				});
				t.start(); 
			}
		}
	}
	
}
