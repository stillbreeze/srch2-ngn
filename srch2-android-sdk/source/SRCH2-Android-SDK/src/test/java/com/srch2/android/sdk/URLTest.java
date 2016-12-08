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

import android.net.Uri;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

import java.io.UnsupportedEncodingException;

@Config(emulateSdk = 18)
@RunWith(RobolectricTestRunner.class)
public class URLTest {

    static {
        PrepareEngine.prepareEngine();
        System.out.println("OAUTH KEY::::" + SRCH2Engine.conf.getAuthorizationKey());
    }

    private static final String QUERY_KEY_WORDS = "android servi";
    private static final String EXPECTED_QUERY_RAWSTRING = "q=android~ AND servi*~";
    private static final String EXPECTED_QUERY_FORMAT = UrlBuilder.getURLEncodedString(EXPECTED_QUERY_RAWSTRING);
    private static final IndexInternal TEST_CORE = SRCH2Engine.getConfig().indexableMap.values().iterator().next().indexInternal;
    private static final String DEFAULT_CORE_NAME = TEST_CORE.getIndexCoreName();


    private static final String ONE_DOC_ID = "id1";

    private static final String HOST_URL = "http://" + SRCH2Configuration.HOSTNAME
            + ":" + SRCH2Engine.getConfig().getPort() + "/";
    private static final String EXPECTED_DELETE_ONE = HOST_URL + DEFAULT_CORE_NAME
            + "/docs?OAuth=" + PrepareEngine.OAUTH + "&" + TEST_CORE.getConf().schema.uniqueKey + "=" + ONE_DOC_ID;

    private static final String EXPECTED_INFO = HOST_URL + DEFAULT_CORE_NAME + "/info?OAuth=" + PrepareEngine.OAUTH;
    private static final String EXPECTED_SAVE = HOST_URL + DEFAULT_CORE_NAME + "/save?OAuth=" + PrepareEngine.OAUTH;
    private static final String EXPECTED_SEARCH_ALL = HOST_URL + "_all/search?OAuth=" + PrepareEngine.OAUTH + "&"
            + EXPECTED_QUERY_FORMAT;
    private static final String EXPECTED_SEARCH_ONE = HOST_URL + DEFAULT_CORE_NAME
            + "/search?OAuth=" + PrepareEngine.OAUTH + "&" + EXPECTED_QUERY_FORMAT;
    private static final String EXPECTED_SHUTDOWN = HOST_URL + "_all/shutdown?OAuth=" + PrepareEngine.OAUTH;
    private static final String EXPECTED_UPDATE = HOST_URL + DEFAULT_CORE_NAME
            + "/update?OAuth=" + PrepareEngine.OAUTH;
    private static final String EXPECTED_INSERT = HOST_URL + DEFAULT_CORE_NAME
            + "/docs?OAuth=" + PrepareEngine.OAUTH;

    private static final String EXPECTED_GETDOC = HOST_URL + DEFAULT_CORE_NAME
            + "/search?OAuth=" + PrepareEngine.OAUTH + "&docid=" + ONE_DOC_ID;


    @Test
    public void testDefaultQueryFormat() throws UnsupportedEncodingException {
        Assert.assertEquals(EXPECTED_QUERY_RAWSTRING,
                IndexInternal.formatDefaultQueryURL(QUERY_KEY_WORDS));
    }

    @Test
    public void testDeleteURL() {
        Assert.assertEquals(
                EXPECTED_DELETE_ONE,
                UrlBuilder.getDeleteUrl(SRCH2Engine.getConfig(),
                        TEST_CORE.getConf(), ONE_DOC_ID)
                        .toString());
    }

    @Test
    public void testInfoURL() {
        Assert.assertEquals(
                EXPECTED_INFO,
                UrlBuilder.getInfoUrl(SRCH2Engine.getConfig(),
                        TEST_CORE.getConf()).toString());
    }

    @Test
    public void testSaveURL() {
        Assert.assertEquals(
                EXPECTED_SAVE,
                UrlBuilder.getSaveUrl(SRCH2Engine.getConfig(),
                        TEST_CORE.getConf()).toString());
    }

    @Test
    public void testSearchAll() throws UnsupportedEncodingException {
        Assert.assertEquals(
                EXPECTED_SEARCH_ALL,
                UrlBuilder.getSearchUrl(SRCH2Engine.getConfig(), null,
                        IndexInternal.formatDefaultQueryURL(QUERY_KEY_WORDS))
                        .toString());
    }

    @Test
    public void testSearchOne() throws UnsupportedEncodingException {
        Assert.assertEquals(
                EXPECTED_SEARCH_ONE,
                UrlBuilder.getSearchUrl(SRCH2Engine.getConfig(),
                        TEST_CORE.getConf(),
                        IndexInternal.formatDefaultQueryURL(QUERY_KEY_WORDS))
                        .toString());
    }

    @Test
    public void testShutDown() {
        Assert.assertEquals(EXPECTED_SHUTDOWN,
                UrlBuilder.getShutDownUrl(SRCH2Engine.getConfig())
                        .toString());
    }

    @Test
    public void testUpdate() {
        Assert.assertEquals(
                EXPECTED_UPDATE,
                UrlBuilder.getUpdateUrl(SRCH2Engine.getConfig(),
                        TEST_CORE.getConf()).toString());
    }

    @Test
    public void testInsert() {
        Assert.assertEquals(
                EXPECTED_INSERT,
                UrlBuilder.getInsertUrl(SRCH2Engine.getConfig(),
                        TEST_CORE.getConf()).toString());
    }

    @Test
    public void testGetDocId() {
        Assert.assertEquals(
                EXPECTED_GETDOC,
                UrlBuilder.getGetDocUrl(SRCH2Engine.getConfig(),
                        TEST_CORE.getConf(), ONE_DOC_ID)
                        .toString());
    }
}
