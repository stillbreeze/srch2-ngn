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

import android.annotation.TargetApi;
import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.IBinder;
import android.os.SystemClock;

import java.io.*;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.Timer;
import java.util.TimerTask;
import java.util.concurrent.Semaphore;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * At the core of the SRCH2 Android SDK is the SRCH2 search server. This service runs remotely and hosts the
 * process that the SRCH2 search server resides in. If you are using the {@code SRCH2-Android-SDK.aar} this
 * service will be automatically included in the application's manifest. The life cycle of this remote service
 * is controlled by broadcast receivers, and it is not bound to by the {@code SRCH2Engine}. Users of the
 * SRCH2 Android SDK should have no reason to modify this service in any way.
 */
final public class SRCH2Service extends Service implements AutoPing.ValidatePingCommandCallback {

    /* README: see diagram explaning the logic of this class @
    https://docs.google.com/a/srch2.com/drawings/d/1mIkQR8ax-TOG0JQHPqmzyWYjBMd0vbXpwk80Zv4EjyE/edit?usp=sharing
    or look for file SRCH2-Android-SDK-App-Service-Server-Interaction-Diagram.png at project root. */

    private static final String TAG = "Exe-Service";

    @Override
    public void validateIfSRCH2EngineAlive() {
        signalSRCH2EngineIsAlive(true);
    }

    private class ExecutableServiceBroadcastReciever extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            Cat.d(TAG, "ExecutableServiceBroadcastReciever -- onRecieve");
            String value = intent.getStringExtra(IPCConstants.INTENT_KEY_BROADCAST_ACTION);
            if (value.equals(IPCConstants.INTENT_VALUE_BROADCAST_ACTION_START_AWAITING_SHUTDOWN)) {
                Cat.d(TAG, "ExecutableServiceBroadcastReciever -- onRecieve -- startAwaitingShutdown");
                startAwaitingShutdown();
            } else if (value.equals(IPCConstants.INTENT_VALUE_BROADCAST_ACTION_VALIDATE_SRCH2ENGINE_ALIVE)) {
                Cat.d(TAG, "ExecutableServiceBroadcastReciever -- validated alive restarting server");
                SRCH2EngineSignaledThatItIsAlive.set(true);
                restartServer();
            } else if (value.equals(IPCConstants.INTENT_VALUE_BROADCAST_ACTION_VALIDATE_SRCH2ENGINE_ALIVE_FOR_PING)) {
                Cat.d(TAG, "ExecutableServiceBroadcastReciever -- recieved validation for doing ping");
                AutoPing.doPing();
            }
        }
    }


    Process executableProcess;

    private AtomicBoolean SRCH2EngineSignaledThatItIsAlive;

    private ExecutableServiceBroadcastReciever incomingIntentReciever;

    private String executableProcessPath;
    private String xmlConfigurationFilePath;
    private int executablePortNumber;
    private String executableShutdownUrlString;
    private String executableOAuthLiteral;
    private String executablePingUrl;

    private AtomicBoolean isAwaitingShutdown;
    private Timer awaitingShutdownTimer;
    private static final int DEFAULT_TIME_TO_WAIT_FOR_SHUTDOWN_MS = (IPCConstants.HEART_BEAT_SERVER_CORE_SHUTDOWN_DELAY_SECONDS * 1000) / 2;
    private static int TIME_TO_WAIT_FOR_SHUTDOWN_MS = DEFAULT_TIME_TO_WAIT_FOR_SHUTDOWN_MS;
    static int getTimeToWaitForShutdown() { return TIME_TO_WAIT_FOR_SHUTDOWN_MS; }

    private AtomicBoolean isShuttingDown;
    private Semaphore shutdownMutex;

    private static final String PREFERENCES_NAME_SERVER_STARTED_LOG = "srch2-server-started-log";
    private static final String PREFERENCES_KEY_SERVER_LOG_SHUTDOWN_URLS = "srch2-server-log-shutdown-urls";
    private static final String PREFERENCES_KEY_SERVER_LOG_USED_PORT_NUMBER = "srch2-server-log-port-number";
    private static final String PREFERENCES_KEY_SERVER_LOG_PREVIOUS_O_AUTH_CODE = "srch2-server-o-auth";
    private static final String PREFERENCES_KEY_SERVER_LOG_EXECUTABLE_PATH = "exe-path";
    private static final String PREFERENCES_KEY_SERVER_LOG_PING_URL = "ping-url";

    private final static String PREFERENCES_DEFAULT_NO_VALUE = "no-value";

    private Context context;

    @Override
    public void onCreate() {
        super.onCreate();
        Cat.d(TAG, "onCreate");
        context = getApplicationContext();
        SRCH2EngineSignaledThatItIsAlive = new AtomicBoolean(false);
        shutdownMutex = new Semaphore(1);
        isAwaitingShutdown = new AtomicBoolean(false);
        isShuttingDown = new AtomicBoolean(false);
        incomingIntentReciever = new ExecutableServiceBroadcastReciever();
        registerReceiver(incomingIntentReciever, IPCConstants.getSRCH2ServiceBroadcastRecieverIntentFilter(getApplicationContext()));
    }

    @Override
    public void onDestroy() {
        Cat.d(TAG, "onDestroy");
        try {
            unregisterReceiver(incomingIntentReciever);
        } catch (IllegalArgumentException ignore) {
        }
        AutoPing.stop();
        if (executableProcess != null) {
            try {
                executableProcess.destroy();
            } catch (Exception e) {
                //ignore
            }

        }
        super.onDestroy();
    }

    @TargetApi(Build.VERSION_CODES.ECLAIR)
    @Override
    public int onStartCommand(final Intent intent, int flags, int startId) {
        Cat.d(TAG, "onStartCommand");
        Thread t = new Thread(new Runnable() {
            @Override
            public void run() {
                resolveLifeCycleAction(intent);
            }
        });
        t.setName("SERVICE ON START COMMAND");
        t.start();
        return Service.START_STICKY;
    }

    private boolean checkIfProcessIsRunningWithoutHavingShutdownCalled() {

        Cat.d(TAG, "checkIfProcessIsRunningWithoutHavingShutdownCalled");

        SharedPreferences sharedpreferences = getSharedPreferences(PREFERENCES_NAME_SERVER_STARTED_LOG, Context.MODE_PRIVATE);
        String shutDownUrlsLiteral = sharedpreferences.getString(PREFERENCES_KEY_SERVER_LOG_SHUTDOWN_URLS, PREFERENCES_DEFAULT_NO_VALUE);
        int portNumber = sharedpreferences.getInt(PREFERENCES_KEY_SERVER_LOG_USED_PORT_NUMBER, 0);
        String oauth = sharedpreferences.getString(PREFERENCES_KEY_SERVER_LOG_PREVIOUS_O_AUTH_CODE, PREFERENCES_DEFAULT_NO_VALUE);
        String executablePath = sharedpreferences.getString(PREFERENCES_KEY_SERVER_LOG_EXECUTABLE_PATH, PREFERENCES_DEFAULT_NO_VALUE);
        String pingUrl = sharedpreferences.getString(PREFERENCES_KEY_SERVER_LOG_PING_URL, PREFERENCES_DEFAULT_NO_VALUE);

        if (!shutDownUrlsLiteral.equals(PREFERENCES_DEFAULT_NO_VALUE) && portNumber != 0
                && !oauth.equals(PREFERENCES_DEFAULT_NO_VALUE) && !executablePath.equals(PREFERENCES_DEFAULT_NO_VALUE)
                    && !pingUrl.equals(PREFERENCES_DEFAULT_NO_VALUE)) {

            executablePortNumber = portNumber;
            executableShutdownUrlString = shutDownUrlsLiteral;
            executableOAuthLiteral = oauth;
            executableProcessPath = executablePath;
            executablePingUrl = pingUrl;
            Cat.d(TAG, "checkIfProcessIsRunningWithoutHavingShutdownCalled was there with values " + portNumber + " " + shutDownUrlsLiteral);
            return true;
        } else {
            Cat.d(TAG, "checkIfProcessIsRunningWithoutHavingShutdownCalled was not there");
            return false;
        }
    }

    private void updateServerLog(int portNumberToPersist, String shutdownUrlToPersist, String oAuth, String executablePath, String pingUrl) {
        Cat.d(TAG, "updateServerLog with port " + portNumberToPersist + " and shutdownurl " + shutdownUrlToPersist);
        SharedPreferences sharedpreferences = getSharedPreferences(PREFERENCES_NAME_SERVER_STARTED_LOG, Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = sharedpreferences.edit();
        editor.putString(PREFERENCES_KEY_SERVER_LOG_SHUTDOWN_URLS, shutdownUrlToPersist);
        editor.putInt(PREFERENCES_KEY_SERVER_LOG_USED_PORT_NUMBER, portNumberToPersist);
        editor.putString(PREFERENCES_KEY_SERVER_LOG_PREVIOUS_O_AUTH_CODE, oAuth);
        editor.putString(PREFERENCES_KEY_SERVER_LOG_EXECUTABLE_PATH, executablePath);
        editor.putString(PREFERENCES_KEY_SERVER_LOG_PING_URL, pingUrl);
        editor.commit();
    }

    static private void clearServerLogEntries(Context context) {
        Cat.d(TAG, "clearServerLogEntries");
        SharedPreferences sharedpreferences = context.getSharedPreferences(PREFERENCES_NAME_SERVER_STARTED_LOG, Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = sharedpreferences.edit();
        editor.remove(PREFERENCES_KEY_SERVER_LOG_SHUTDOWN_URLS);
        editor.remove(PREFERENCES_KEY_SERVER_LOG_USED_PORT_NUMBER);
        editor.remove(PREFERENCES_KEY_SERVER_LOG_PREVIOUS_O_AUTH_CODE);
        editor.remove(PREFERENCES_KEY_SERVER_LOG_EXECUTABLE_PATH);
        editor.remove(PREFERENCES_KEY_SERVER_LOG_PING_URL);
        editor.commit();
    }

    /** Only to be used for testing, use when manually deleting indexes to reset to "fresh state". */
    static void clearServerLogEntriesForTest(Context context) {
        clearServerLogEntries(context);
    }

    private void resolveLifeCycleAction(final Intent startCommandIntent) {
        Cat.d(TAG, "resolveLifeCycleAction");
        if (checkIfProcessIsRunningWithoutHavingShutdownCalled()) {
            Cat.d(TAG, "resolveLifeCycleAction - was running");
            stopAwaitingShutdown();
            if (isShuttingDown.get()) {
                Cat.d(TAG, "resolveLifeCycleAction - was shutting down");
                try {
                    Cat.d(TAG, "resolveLifeCycleAction - acquired mutex");
                    shutdownMutex.acquire();
                } catch (InterruptedException interruptedBySystem) {
                    Cat.ex(TAG, "interrupted while shutting down", interruptedBySystem);
                    Cat.d(TAG, "resolveLifeCycleAction - MUTEX INTERRUPTED");
                    return;
                } finally {
                    Cat.d(TAG, "resolveLifeCycleAction - releasing mutex");
                    shutdownMutex.release();
                }
                Cat.d(TAG, "resolveLifeCycleAction - finished blocking on mutex, starting exectuabe...");
                startExecutable(startCommandIntent);
            } else {
                Cat.d(TAG, "resolveLifeCycleAction - was running without shutdown CONTINUE running");
                Cat.d(TAG, "resolveLifeCycleAction - CONTINUE RUNNING exe path " + executableProcessPath);

                signalSRCH2EngineToProceed(executablePortNumber, executableOAuthLiteral);
            }
        } else {
            Cat.d(TAG, "resolveLifeCycleAction - was not running starting");
            startExecutable(startCommandIntent);
        }
    }

    private void signalSRCH2EngineToProceed(int portNumberForSRCH2EngineToReuse, String oAuthCodeForSRCH2EngineToReuse) {
        Cat.d(TAG, "signalSRCH2EngineToProceed");
        Intent i = new Intent(IPCConstants.getSRCH2EngineBroadcastRecieverIntentAction(getApplicationContext()));
        i.putExtra(IPCConstants.INTENT_KEY_BROADCAST_ACTION, IPCConstants.INTENT_VALUE_BROADCAST_ACTION_ENGINE_STARTED_PROCEED);
        i.putExtra(IPCConstants.INTENT_KEY_PORT_NUMBER, portNumberForSRCH2EngineToReuse);
        i.putExtra(IPCConstants.INTENT_KEY_OAUTH, oAuthCodeForSRCH2EngineToReuse);
        AutoPing.start(this, executablePingUrl);
        sendBroadcast(i);
    }

    private void signalSRCH2EngineToResume(int portNumberForSRCH2EngineToReuse, String oAuthCodeForSRCH2EngineToReuse) {
        Cat.d(TAG, "signalSRCH2EngineToResume");
        Intent i = new Intent(IPCConstants.getSRCH2EngineBroadcastRecieverIntentAction(getApplicationContext()));
        i.putExtra(IPCConstants.INTENT_KEY_BROADCAST_ACTION, IPCConstants.INTENT_VALUE_BROADCAST_ACTION_ENGINE_CRASHED_BUT_CAN_RESUME);
        i.putExtra(IPCConstants.INTENT_KEY_PORT_NUMBER, portNumberForSRCH2EngineToReuse);
        i.putExtra(IPCConstants.INTENT_KEY_OAUTH, oAuthCodeForSRCH2EngineToReuse);
        AutoPing.start(this, executablePingUrl);
        sendBroadcast(i);
    }

    private void signalSRCH2EngineIsAlive(boolean forPing) {
        Cat.d(TAG, "signalSRCH2EngineForPingValidation");
        Intent i = new Intent(IPCConstants.getSRCH2EngineBroadcastRecieverIntentAction(getApplicationContext()));
        if (forPing) {
            i.putExtra(IPCConstants.INTENT_KEY_BROADCAST_ACTION, IPCConstants.INTENT_VALUE_BROADCAST_ACTION_VALIDATE_SRCH2ENGINE_ALIVE_FOR_PING);
        } else {
            i.putExtra(IPCConstants.INTENT_KEY_BROADCAST_ACTION, IPCConstants.INTENT_VALUE_BROADCAST_ACTION_VALIDATE_SRCH2ENGINE_ALIVE);
        }

        sendBroadcast(i);
    }

    private void stopAwaitingShutdown() {
        Cat.d(TAG, "stopAwaitingShutdown");
        if (awaitingShutdownTimer != null) {
            awaitingShutdownTimer.cancel();
            awaitingShutdownTimer = null;
        }
        isAwaitingShutdown.set(false);
    }

    private void startAwaitingShutdown() {
        Cat.d(TAG, "startAwaitingShutdown START AWAITING shutdown!");
        isAwaitingShutdown.set(true);
        awaitingShutdownTimer = new Timer();
        awaitingShutdownTimer.schedule(new TimerTask() {

            @Override
            public void run() {
                Thread.currentThread().setName("SHUTDOWN THREAD");
                if (!isAwaitingShutdown.get()) {
                    return;
                }
                Cat.d(TAG, "shutting down begin " + executableProcessPath);
                if (executableProcessPath != null) {
                    isShuttingDown.set(true);

                    try {
                        Cat.d(TAG, "shutting down begin - shutdownMutex acquired");
                        shutdownMutex.acquire();
                    } catch (InterruptedException ignore) {
                        Cat.d(TAG, "shutdowning MUTEX INTERRUPTED!");
                    }
                    AutoPing.stop();
                    try {

                        doShutdownNetworkCall();
                        Cat.d(TAG, "shutting down begin - about to enter while loop path is " + executableProcessPath);

                        int totalSleepTime = 0;
                        while (ps(executableProcessPath)) {
                            if (totalSleepTime % 400 == 0) {
                                Cat.d(TAG, "shutting down whiling while the ps is true");
                            }
                            if (totalSleepTime > TIME_TO_WAIT_FOR_SHUTDOWN_MS) {
                                Cat.d(TAG, "shuting down whileing ps is true breaking BREAKING");
                                break;
                            }

                            try {
                                Thread.currentThread().sleep(200);
                                totalSleepTime += 200;
                            } catch (InterruptedException e) {
                                Cat.d(TAG, "shutting down whiling while the ps is true INTERRUPED");
                                Cat.ex(TAG, "shutting down whiling while ", e);
                            }
                        }
                        clearServerLogEntries(context);
                        } finally {
                            Cat.d(TAG, "shutting down finally block finished - releasing and setting to false");
                            shutdownMutex.release();
                            isShuttingDown.set(false);
                        }
                Cat.d(TAG, "shutting down finished - about to check to stop self");

                checkToStopSelf();
                }
            }
        }, TIME_TO_WAIT_FOR_SHUTDOWN_MS);
    }

    private void doShutdownNetworkCall() {
        Cat.d(TAG, "doShutdownNetworkCall");
        URL url = null;
        try {
            url = new URL(executableShutdownUrlString);
        } catch (MalformedURLException ignore) {
        }
        HttpURLConnection connection = null;
        try {
            connection = (HttpURLConnection) url.openConnection();
            connection.setDoOutput(true);
            connection.setRequestMethod("PUT");
            connection.setUseCaches(false);
            connection.connect();
            OutputStreamWriter out = new OutputStreamWriter(
                    connection.getOutputStream());
            out.write("X");
            out.close();
            int responseCode = connection.getResponseCode();
            Cat.d(TAG, "doShutdownNetworkCall - responseCode " + responseCode);
            String response = "";
            if (connection.getInputStream() != null) {
                response = readInputStream(connection.getInputStream());
            } else if (connection.getErrorStream() != null) {
                response = readInputStream(connection.getErrorStream());
            }
            Cat.d(TAG, "doShutdownNetworkCall response " + response);
        } catch (IOException networkError) {
            Cat.ex(TAG, "shutdown network call IOException", networkError);
            Cat.d(TAG, "network error WHILE shutting down " + networkError.getMessage());
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
        }

        if (executableProcess != null) {
            executableProcess.destroy();
        }
    }

    private void checkToStopSelf() {
        Cat.d(TAG, "checkToStopSelf");
        if (isAwaitingShutdown.get()) {
            Cat.d(TAG, "checkToStopSelf - stopping service");
            stopSelf();
        }
    }

    /** For testing purposes only */
    void stopService() {
        stopSelf();
    }

    private void startExecutable(Intent startCommandIntent) {
        Cat.d(TAG, "startExecutable");
        executablePortNumber = startCommandIntent.getIntExtra(IPCConstants.INTENT_KEY_PORT_NUMBER, 0);
        executableShutdownUrlString = startCommandIntent.getStringExtra(IPCConstants.INTENT_KEY_SHUTDOWN_URL);
        executableOAuthLiteral = startCommandIntent.getStringExtra(IPCConstants.INTENT_KEY_OAUTH);
        boolean isDebugAndTestingMode = startCommandIntent.getBooleanExtra(IPCConstants.INTENT_KEY_IS_DEBUG_AND_TESTING_MODE, false);
        TIME_TO_WAIT_FOR_SHUTDOWN_MS = isDebugAndTestingMode ? IPCConstants.DEBUG_AND_TESTING_MODE_QUICK_SHUTDOWN_DELAY : DEFAULT_TIME_TO_WAIT_FOR_SHUTDOWN_MS;
        executablePingUrl = startCommandIntent.getStringExtra(IPCConstants.INTENT_KEY_PING_URL);
        autoInstallCoreFilesAndOverwriteXMLConfigurationFile(startCommandIntent.getStringExtra(IPCConstants.INTENT_KEY_XML_CONFIGURATION_FILE_LITERAL));
        startRunningExecutable(executablePortNumber, executableShutdownUrlString, executableOAuthLiteral, executablePingUrl);
        Cat.d(TAG, "startExecutable port number " + executablePortNumber);
        Cat.d(TAG, "startExecutable shutdown string " + executableShutdownUrlString);
        Cat.d(TAG, "startExecutable pingUrl " + executablePingUrl);
        signalSRCH2EngineToProceed(executablePortNumber, executableOAuthLiteral);
    }

    private void startRunningExecutable(final int portBeingUsedToStartService, final String shutDownUrl, final String oAuthCode, final String pingUrl) {
        Cat.d(TAG, "startRunningExecutable");
        Thread t = new Thread(new Runnable() {
            @Override
            public void run() {
                ProcessBuilder pb = new ProcessBuilder(executableProcessPath, "--config-file", xmlConfigurationFilePath);

                try {
                    updateServerLog(portBeingUsedToStartService, shutDownUrl, oAuthCode, executableProcessPath, pingUrl);
                    Cat.d(TAG, "startRunningExecutable - starting process");
                    executableProcess = pb.start();

                    Cat.d(TAG, "startRunningExe - after pbstart isShuttingDown is " + isShuttingDown.get());
                    if (executableProcess.getInputStream() != null) {
                        Cat.d(TAG, "PRINTING INPUT STREAM\n" + readInputStream(executableProcess.getInputStream()));
                    } else {
                        Cat.d(TAG, "NO INPUT STREAM from process");
                    }
                    if (executableProcess.getErrorStream() != null) {
                        Cat.d(TAG, "PRINTING ERROR STREAM\n" + readInputStream(executableProcess.getErrorStream()));
                    } else {
                        Cat.d(TAG, "NO ERROR STREAM from process");
                    }
                   // executableProcess.destroy();
                    if (!isShuttingDown.get()) {
                        clearServerLogEntries(context);
                        Cat.d(TAG, "startRunningExe - after executableProcess.destory engine may have crash validating ... ");
                        AutoPing.interrupt();
                        SRCH2EngineSignaledThatItIsAlive.set(false);
                        signalSRCH2EngineIsAlive(false);

                        long timeToWaitForSRCH2EngineAliveCallback = SystemClock.uptimeMillis() + 3000;
                        while (SystemClock.uptimeMillis() < timeToWaitForSRCH2EngineAliveCallback && !SRCH2EngineSignaledThatItIsAlive.get()) {
                            Cat.d(TAG, "startRunningExe - waiting for validation to restart waited");
                            try {
                                Thread.sleep(250);
                            } catch (InterruptedException e) {
                            }
                        }
                        if (!SRCH2EngineSignaledThatItIsAlive.get()) {
                            Cat.d(TAG, "startRunningExe - did not get validation should be stopping self");
                            stopSelf();
                        }
                        SRCH2EngineSignaledThatItIsAlive.set(false);
                    }
                    Cat.d(TAG, "startRunningExe - after executableProcess.destory isShuttingDown is " + isShuttingDown.get());
                } catch (IOException e) {
                    Cat.d(TAG, "IOEXCEPTION starting executable!");
                    Cat.ex(TAG, "starting executable io error", e);
                } catch (Exception e) {
                    Cat.d(TAG, "EXCEPTION starting executable!");
                    Cat.ex(TAG, "starting executable general error", e);
                }
            }
        });
        t.setName("EXECUTABLE PROCESS THREAD");
        t.start();
    }

    private void restartServer() {
        Cat.d(TAG, "restarting server");
        startRunningExecutable(executablePortNumber, executableShutdownUrlString, executableOAuthLiteral, executablePingUrl);
        signalSRCH2EngineToResume(executablePortNumber, executableOAuthLiteral);
    }

    @TargetApi(Build.VERSION_CODES.DONUT)
    private void autoInstallCoreFilesAndOverwriteXMLConfigurationFile(String xmlConfigurationFileLiteral) { //verify chmod bitmask to use
        Cat.d(TAG, "autoInstallCoreFilesAndOverwriteXMLConfigurationFile");
        final Context c = getApplicationContext();
        String dataDirectoryFilePath = c.getApplicationInfo().dataDir;
        File filesDirectory = c.getFilesDir();

        try {
            File binDirectory = new File(dataDirectoryFilePath, "bin");
            if (!binDirectory.exists()) {
                binDirectory.mkdir();
                chmod("755", binDirectory.getAbsolutePath());
            }

            File srch2RootDirectory = new File(filesDirectory, "srch2");
            if (!srch2RootDirectory.exists()) {
                srch2RootDirectory.mkdir();
                chmod("755", srch2RootDirectory.getAbsolutePath());
            }

            File stopWords = new File(srch2RootDirectory, "stop-words.txt");
            if (!stopWords.exists()) {
                InputStream sourceFile = c.getResources().openRawResource(R.raw.stopwords);
                FileOutputStream destinationFile = new FileOutputStream(stopWords);
                copyStream(destinationFile, sourceFile);
            }

            File executableBinary = new File(binDirectory, "srch2ngn.exe");
            if (!executableBinary.exists()) {
                InputStream sourceFile = c.getResources().openRawResource(R.raw.srch2eng4415android022);
                FileOutputStream destinationFile = new FileOutputStream(executableBinary);
                copyStream(destinationFile, sourceFile);
                chmod("775", executableBinary.getAbsolutePath());
            }
            executableProcessPath = executableBinary.getAbsolutePath();
            Cat.d(TAG, "autoInstallCoreFilesAndOverwriteXMLConfigurationFile - executableProcessPath " + executableProcessPath);

            File sqliteSharedLibrary = new File(binDirectory, "sqliteconn.so");
            if (!sqliteSharedLibrary.exists()) {
                InputStream sourceFile = c.getResources().openRawResource(R.raw.libsq4415android022);
                FileOutputStream destinationFile = new FileOutputStream(sqliteSharedLibrary);
                copyStream(destinationFile, sourceFile);
                chmod("775", executableBinary.getAbsolutePath());
            }

            File configFile = new File(srch2RootDirectory, "srch2-config.xml");
            if (!configFile.exists()) {
                configFile.createNewFile();
            }
            BufferedWriter buf = new BufferedWriter(new FileWriter(configFile));
            buf.write(xmlConfigurationFileLiteral);
            buf.close();
            xmlConfigurationFilePath = configFile.getAbsolutePath();
            Cat.d(TAG, "autoInstallCoreFilesAndOverwriteXMLConfigurationFile - xmlConfigurationFilePath " + xmlConfigurationFilePath);
        } catch (Exception ignore) {
            Cat.ex(TAG, "while installing files", ignore);
        }
    }

    private boolean ps(String executablePathToCheck) {
        boolean isAlreadyRunning = false;
        ProcessBuilder checkPb = new ProcessBuilder("ps");
        Process checkP;
        try {
            checkP = checkPb.start();
            String listing = readInputStream(checkP.getInputStream());
            if (listing.contains(executablePathToCheck)) {
                isAlreadyRunning = true;
            } else {
                // not already running
            }
        } catch (IOException ignore) {
        }
        return isAlreadyRunning;
    }

    private void copyStream(OutputStream target, InputStream source) throws IOException {
        byte[] buffer = new byte[4096];
        int bytesRead;
        while ((bytesRead = source.read(buffer)) >= 0) {
            target.write(buffer, 0, bytesRead);
        }
        target.close();
    }

    private void chmod(String... args) throws IOException {
        String[] cmdline = new String[args.length + 1];
        cmdline[0] = "/system/bin/chmod";
        System.arraycopy(args, 0, cmdline, 1, args.length);
        new ProcessBuilder(cmdline).start();
    }

    private static String readInputStream(InputStream source) throws IOException {
        if (source == null) {
            return "NULL SOURCE INPUT STREAM";
        }
        StringBuilder sb = new StringBuilder();
        String line;
        BufferedReader reader = null;
        try {
            reader = new BufferedReader(new InputStreamReader(source, "UTF-8"));
            while ((line = reader.readLine()) != null) {
                sb.append(line);
            }
        } finally {
            if (reader != null) {
                reader.close();
            }
        }
        return sb.toString();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
