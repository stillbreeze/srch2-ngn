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

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

import java.io.*;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;

@Config(emulateSdk = 18)
@RunWith(RobolectricTestRunner.class)
public class ConfigurationTest {


    private static final String SRCH2BIN_PROPERTY = "linuxSRCH2bin";

    static {
        PrepareEngine.prepareEngine();
    }

    public static void outputStream(InputStream is, PrintStream os)
            throws IOException {
        InputStreamReader isr = new InputStreamReader(is);
        BufferedReader br = new BufferedReader(isr);
        String line;

        while ((line = br.readLine()) != null) {
            os.println(line);
        }
    }

    @Test
    public void testWriteTo() throws FileNotFoundException,
            UnsupportedEncodingException {
        SRCH2Engine.getConfig().writeConfigurationFileToDisk(System.getProperty("java.io.tmpdir") + File.separator + "srch2-config-4.xml");
    }

    @Test
    public void testConfiguration() {
        ArrayList<IndexableCore> idxs = new ArrayList<IndexableCore>();
        idxs.add(PrepareEngine.movieIndex);
        SRCH2Configuration config = new SRCH2Configuration(idxs);
        Assert.assertEquals(config.indexableMap.get(PrepareEngine.movieIndex.getIndexName()).indexInternal.indexDescription.type, IndexDescription.IndexableType.Default);

        idxs.clear();
        idxs.add(PrepareEngine.musicIndex);
        SRCH2Configuration config2 = new SRCH2Configuration(idxs);
        Assert.assertEquals(config2.indexableMap.get(PrepareEngine.musicIndex.getIndexName()).indexInternal.indexDescription.type, IndexDescription.IndexableType.Default);
    }

    @Test
    public void testConfigurationGeoIndex() {
        ArrayList<IndexableCore> idxs = new ArrayList<IndexableCore>();
        idxs.add(PrepareEngine.geoIndex);
        SRCH2Configuration config = new SRCH2Configuration(idxs);
    }

    @Test
    public void testConfigurationMultipleIndex() {
        ArrayList<IndexableCore> idxs = new ArrayList<IndexableCore>();
        idxs.add(PrepareEngine.geoIndex);
        idxs.add(PrepareEngine.musicIndex);
        SRCH2Configuration config = new SRCH2Configuration(idxs);
        Assert.assertEquals(config.indexableMap.get(PrepareEngine.geoIndex.getIndexName())
                .indexInternal.indexDescription.type, IndexDescription.IndexableType.Default);
        Assert.assertEquals(config.indexableMap.get(PrepareEngine.musicIndex.getIndexName())
                .indexInternal.indexDescription.type, IndexDescription.IndexableType.Default);
    }

    @Test
    public void testConfigurationMultipleIndexIncludingDatabaseIndex() {
        ArrayList<IndexableCore> idxs = new ArrayList<IndexableCore>();
        idxs.add(PrepareEngine.geoIndex);
        idxs.add(PrepareEngine.musicIndex);

        SRCH2Configuration config = new SRCH2Configuration(idxs);
        Assert.assertEquals(config.indexableMap.get(PrepareEngine.geoIndex.getIndexName())
                .indexInternal.indexDescription.type, IndexDescription.IndexableType.Default);
        Assert.assertEquals(config.indexableMap.get(PrepareEngine.musicIndex.getIndexName())
                .indexInternal.indexDescription.type, IndexDescription.IndexableType.Default);

    }

    @Test
    public void testAuthorization() {
        ArrayList<IndexableCore> idxs = new ArrayList<IndexableCore>();
        idxs.add(PrepareEngine.movieIndex);
        idxs.add(PrepareEngine.musicIndex);
        SRCH2Configuration config = new SRCH2Configuration(idxs);
        config.setAuthorizationKey("myAuthorizationKey");
        Assert.assertEquals(config.getAuthorizationKey(), "myAuthorizationKey");
    }

    @Test(expected = NullPointerException.class)
    public void testAuthorizationNullException() {
        ArrayList<IndexableCore> idxs = new ArrayList<IndexableCore>();
        idxs.add(PrepareEngine.movieIndex);
        idxs.add(PrepareEngine.musicIndex);
        SRCH2Configuration config = new SRCH2Configuration(idxs);
        config.setAuthorizationKey(null);
    }

    @Test
    public void testSet() {
        Assert.assertEquals(PrepareEngine.DEFAULT_SRCH2SERVER_PORT, SRCH2Engine.getConfig().getPort());
        Assert.assertEquals(PrepareEngine.DEFAULT_SRCH2HOME_PATH + File.separator + "srch2/", SRCH2Engine.getConfig().getSRCH2Home());
        String confXML = SRCH2Configuration.generateConfigurationFileString(SRCH2Engine.getConfig());
        Assert.assertTrue(confXML.contains("<listeningHostname>"
                + SRCH2Configuration.HOSTNAME + "</listeningHostname>"));
        Assert.assertTrue(confXML.contains("<listeningPort>"
                + PrepareEngine.DEFAULT_SRCH2SERVER_PORT + "</listeningPort>"));
    }

    @Test
    public void startEngineTest() throws IOException, InterruptedException {
        final String configPath = System.getProperty("java.io.tmpdir") + File.separator + "srch2-config.xml";
        SRCH2Engine.getConfig().writeConfigurationFileToDisk(configPath);
        final String srch2bin = System.getProperty(SRCH2BIN_PROPERTY);
        if (srch2bin == null) {
            return;
        }

        Thread serverThread = new Thread() {
            public void run() {
                String args = "--config=" + configPath;
                Process srch2Process;
                try {
                    srch2Process = new ProcessBuilder(srch2bin, args).start();
                    outputStream(srch2Process.getInputStream(), System.out);
                    outputStream(srch2Process.getErrorStream(), System.err);

                    srch2Process.waitFor();

                    System.out.println(srch2Process.exitValue());
                } catch (IOException e) {
                    e.printStackTrace();
                    Assert.assertFalse(true);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                    Assert.assertFalse(true);
                }

            }
        };
        serverThread.start();

        Thread.sleep(1000);

        InputStream is = null;
        HttpURLConnection connection = null;
        String infoResponseLiteral = null;
        int responseCode = HttpTask.RESTfulResponseTags.FAILED_TO_CONNECT_RESPONSE_CODE;
        try {
            URL url = UrlBuilder.getShutDownUrl(SRCH2Engine.getConfig());
            System.out.println("request url :" + url.toExternalForm());
            connection = (HttpURLConnection) url.openConnection();

            connection.setDoOutput(true);
            connection.setRequestMethod("PUT");
            OutputStreamWriter out = new OutputStreamWriter(
                    connection.getOutputStream());
            out.write("X");
            out.close();
            connection.getInputStream();
            connection.setConnectTimeout(2000);
            connection.setReadTimeout(2000);
            connection.connect();

            responseCode = connection.getResponseCode();
            if (connection.getErrorStream() != null) {
                System.err.println(HttpTask.readInputStream(connection
                        .getErrorStream()));
            }
        } catch (IOException networkError) {
            System.err.println("Error!");
            infoResponseLiteral = networkError.getMessage();
            networkError.printStackTrace();
            Assert.fail();
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
            if (is != null) {
                try {
                    is.close();
                } catch (IOException ignore) {
                }
            }
        }
        System.out.println("finally:" + responseCode + " "
                + infoResponseLiteral);
    }
}
