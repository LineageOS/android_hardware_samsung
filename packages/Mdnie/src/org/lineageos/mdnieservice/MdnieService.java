/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.mdnieservice;

import android.accessibilityservice.AccessibilityService;
import android.accessibilityservice.AccessibilityServiceInfo;
import android.app.ActivityManager;
import android.app.KeyguardManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.SystemClock;
import android.util.Log;
import android.view.accessibility.AccessibilityEvent;

import org.lineageos.internal.util.FileUtils;

import java.util.List;

public class MdnieService extends AccessibilityService {

    private static final String TAG = "MdnieService";

    // Tune these per-device if needed; Samsung loads them from resources.
    private static final int DEBOUNCE_UI_MS      =   0;  // instant – launcher / default
    private static final int DEBOUNCE_VIDEO_MS   = 400;  // ACTION_VIDEO_APP_STATE_IN
    private static final int DEBOUNCE_CAMERA_MS  = 200;  // IS_CAMERA_APP_DEBOUNCE
    private static final int DEBOUNCE_GALLERY_MS = 200;  // ACTION_DETAIL_VIEW_STATE_IN
    private static final int DEBOUNCE_GENERIC_MS = 100;  // navigation, video-call, etc.
    private static final int DEBOUNCE_RESCAN_MS  = 300;  // foreground-rescan guard

    private static final int MSG_RESCAN         =  1;  // debounced foreground re-query
    private static final int MSG_SET_UI         =  2;  // write scenario 0
    private static final int MSG_SET_GALLERY    =  4;  // write scenario 6
    private static final int MSG_SET_CAMERA     =  5;  // write scenario 4
    private static final int MSG_SET_VIDEO      =  6;  // write scenario 1
    private static final int MSG_SET_NAVIGATION = 11;  // write scenario 5
    private static final int MSG_SET_VCALL      = 12;  // write scenario 7

    // ── Scenario integer strings written to the sysfs node
    private static final String SCENARIO_UI        = "0";
    private static final String SCENARIO_VIDEO     = "1";
    private static final String SCENARIO_CAMERA    = "4";
    private static final String SCENARIO_NAVIGATION = "5";
    private static final String SCENARIO_GALLERY   = "6";
    private static final String SCENARIO_VCALL     = "7";

    private static final String[] IGNORED_PACKAGES = {
    };

    private String  mScenarioFile   = null;
    private String  mCurrentPackage = null;

    private String  mLastWritten    = null;

    private boolean mScreenStateOn  = true;

    private boolean mLockScreenOn   = false;

    private boolean mUIScenarioEnabled         = false;
    private boolean mVideoScenarioEnabled      = false;
    private boolean mCameraScenarioEnabled     = false;
    private boolean mGalleryScenarioEnabled    = false;
    private boolean mNavigationScenarioEnabled = false;
    private boolean mVCallScenarioEnabled      = false;

    private KeyguardManager mKeyguardManager;
    private ActivityManager mActivityManager;
    private Handler         mHandler;

    private final BroadcastReceiver mScreenReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            final String action = intent.getAction();
            if (action == null) return;

            switch (action) {

                case Intent.ACTION_SCREEN_ON:
                    mScreenStateOn = true;
                    if (mKeyguardManager != null && mKeyguardManager.isKeyguardLocked()) {
                        mLockScreenOn = true;
                    }
                    Log.d(TAG, "Screen ON – locked=" + mLockScreenOn);
                    if (mLockScreenOn) {
                        applyScenarioNow(SCENARIO_UI);
                    }
                    break;

                case Intent.ACTION_SCREEN_OFF:
                    mScreenStateOn = false;
                    removeAllPendingMessages();
                    applyScenarioNow(Constants.DEFAULT_MDNIE_SCENARIO);
                    Log.d(TAG, "Screen OFF – scenario reset to default");
                    break;

                case Intent.ACTION_USER_PRESENT:
                    mLockScreenOn  = false;
                    mScreenStateOn = true;
                    Log.d(TAG, "User present – scheduling foreground rescan");
                    scheduleRescan();
                    break;
            }
        }
    };

    @Override
    public void onServiceConnected() {
        mScenarioFile = getResources().getString(R.string.mdnie_scenario_sysfs_file);

        final AccessibilityServiceInfo info = new AccessibilityServiceInfo();
        info.eventTypes          = AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED;
        info.feedbackType        = AccessibilityServiceInfo.FEEDBACK_GENERIC;
        info.notificationTimeout = 100;
        setServiceInfo(info);

        mKeyguardManager = (KeyguardManager) getSystemService(Context.KEYGUARD_SERVICE);
        mActivityManager = (ActivityManager)  getSystemService(Context.ACTIVITY_SERVICE);

        seedLastWrittenFromSysfs();

        mHandler = new ScenarioHandler(Looper.getMainLooper());

        final IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_SCREEN_ON);
        filter.addAction(Intent.ACTION_SCREEN_OFF);
        filter.addAction(Intent.ACTION_USER_PRESENT);
        registerReceiver(mScreenReceiver, filter);

        Log.d(TAG, "Service connected – sysfs: " + mScenarioFile
                + "  current=" + mLastWritten);
    }

    @Override
    public void onInterrupt() {
        Log.w(TAG, "Service interrupted");
    }

    @Override
    public boolean onUnbind(Intent intent) {
        try { unregisterReceiver(mScreenReceiver); } catch (Exception ignored) {}
        if (mHandler != null) mHandler.removeCallbacksAndMessages(null);
        applyScenarioNow(Constants.DEFAULT_MDNIE_SCENARIO);
        return super.onUnbind(intent);
    }

    @Override
    public void onAccessibilityEvent(final AccessibilityEvent event) {
        if (event.getEventType() != AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED) return;

        final CharSequence pkgCs = event.getPackageName();
        if (pkgCs == null) return;
        final String packageName = pkgCs.toString();

        if (mLockScreenOn) {
            Log.v(TAG, "Lock screen active – ignoring event for " + packageName);
            return;
        }

        if (!mScreenStateOn) return;

        for (final String ignored : IGNORED_PACKAGES) {
            if (packageName.startsWith(ignored)) return;
        }

        if (packageName.equals(mCurrentPackage)) return;
        mCurrentPackage = packageName;

        dispatchScenarioForPackage(packageName);
    }

    private void dispatchScenarioForPackage(final String packageName) {

/*  To-Do: Fallback to UI mode if Multi-window is active
        if (isMultiWindowActive()) {
            Log.d(TAG, "Multi-window active – collapsing to UI mode");
            if (!mUIScenarioEnabled) {
                scenarioEnableReset();
                mUIScenarioEnabled = true;
                scheduleMessage(MSG_SET_UI, DEBOUNCE_UI_MS);
            }
            return;
        }
*/

        final String scenario = AppScenarioConfig.getScenarioForPackage(this, packageName);

        if (scenario == null) {
            if (!mUIScenarioEnabled) {
                scenarioEnableReset();
                mUIScenarioEnabled = true;
                scheduleMessage(MSG_SET_UI, DEBOUNCE_UI_MS);
            }
            return;
        }

        switch (scenario) {
            case SCENARIO_VIDEO:
                if (!mVideoScenarioEnabled) {
                    scenarioEnableReset();
                    mVideoScenarioEnabled = true;
                    scheduleMessage(MSG_SET_VIDEO, DEBOUNCE_VIDEO_MS);
                }
                break;

            case SCENARIO_CAMERA:
                if (!mCameraScenarioEnabled) {
                    scenarioEnableReset();
                    mCameraScenarioEnabled = true;
                    scheduleMessage(MSG_SET_CAMERA, DEBOUNCE_CAMERA_MS);
                }
                break;

            case SCENARIO_GALLERY:
                if (!mGalleryScenarioEnabled) {
                    scenarioEnableReset();
                    mGalleryScenarioEnabled = true;
                    scheduleMessage(MSG_SET_GALLERY, DEBOUNCE_GALLERY_MS);
                }
                break;

            case SCENARIO_NAVIGATION:
                if (!mNavigationScenarioEnabled) {
                    scenarioEnableReset();
                    mNavigationScenarioEnabled = true;
                    scheduleMessage(MSG_SET_NAVIGATION, DEBOUNCE_GENERIC_MS);
                }
                break;

            case SCENARIO_VCALL:
                if (!mVCallScenarioEnabled) {
                    scenarioEnableReset();
                    mVCallScenarioEnabled = true;
                    scheduleMessage(MSG_SET_VCALL, DEBOUNCE_GENERIC_MS);
                }
                break;

            default:
                // Unrecognised scenario value – fall back to UI.
                if (!mUIScenarioEnabled) {
                    scenarioEnableReset();
                    mUIScenarioEnabled = true;
                    scheduleMessage(MSG_SET_UI, DEBOUNCE_UI_MS);
                }
                break;
        }
    }

    private void scenarioEnableReset() {
        mUIScenarioEnabled         = false;
        mVideoScenarioEnabled      = false;
        mCameraScenarioEnabled     = false;
        mGalleryScenarioEnabled    = false;
        mNavigationScenarioEnabled = false;
        mVCallScenarioEnabled      = false;
    }

    private void scheduleMessage(final int what, final int debounceMs) {
        mHandler.removeMessages(what);
        mHandler.sendEmptyMessageAtTime(what,
                SystemClock.uptimeMillis() + debounceMs);
    }

    private void scheduleRescan() {
        mHandler.removeMessages(MSG_RESCAN);
        mHandler.sendEmptyMessageAtTime(MSG_RESCAN,
                SystemClock.uptimeMillis() + DEBOUNCE_RESCAN_MS);
    }

    private void removeAllPendingMessages() {
        mHandler.removeMessages(MSG_RESCAN);
        mHandler.removeMessages(MSG_SET_UI);
        mHandler.removeMessages(MSG_SET_VIDEO);
        mHandler.removeMessages(MSG_SET_CAMERA);
        mHandler.removeMessages(MSG_SET_GALLERY);
        mHandler.removeMessages(MSG_SET_NAVIGATION);
        mHandler.removeMessages(MSG_SET_VCALL);
    }

    private void applyScenarioNow(final String value) {
        if (mScenarioFile == null || !FileUtils.isFileWritable(mScenarioFile)) return;

        if (value.equals(mLastWritten)) {
            Log.v(TAG, "Scenario " + value + " already active – skipping write");
            return;
        }

        Log.d(TAG, "Writing scenario " + value + " to " + mScenarioFile);
        FileUtils.writeLine(mScenarioFile, value);
        mLastWritten = value;
    }

    private void seedLastWrittenFromSysfs() {
        if (mScenarioFile == null) return;
        try {
            final String current = FileUtils.readOneLine(mScenarioFile);
            if (current != null) {
                mLastWritten = current.trim();
                Log.d(TAG, "Seeded mLastWritten from sysfs: " + mLastWritten);
            }
        } catch (Exception e) {
            Log.w(TAG, "Could not read current scenario from sysfs: " + e.getMessage());
        }
    }

// To-Do: Multi-window detection
/* 
    private boolean isMultiWindowActive() {
    }
*/

    private void rescanForegroundApp() {
        if (!mScreenStateOn) return;
        if (mActivityManager == null) return;

        try {
            final List<ActivityManager.RunningTaskInfo> tasks =
                    mActivityManager.getRunningTasks(1);
            if (tasks == null || tasks.isEmpty()) return;

            final ActivityManager.RunningTaskInfo top = tasks.get(0);
            if (top.topActivity == null) return;

            final String packageName = top.topActivity.getPackageName();
            if (packageName == null) return;

            Log.d(TAG, "Rescan – foreground: " + packageName);

            // Force re-dispatch even if mCurrentPackage has not changed.
            mCurrentPackage = null;
            scenarioEnableReset();
            dispatchScenarioForPackage(packageName);

        } catch (SecurityException e) {
            Log.w(TAG, "getRunningTasks denied – restoring default: " + e.getMessage());
            applyScenarioNow(Constants.DEFAULT_MDNIE_SCENARIO);
        }
    }

    private final class ScenarioHandler extends Handler {

        ScenarioHandler(final Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(final Message msg) {
            switch (msg.what) {
                case MSG_RESCAN:
                    rescanForegroundApp();
                    break;
                case MSG_SET_UI:
                    Log.v(TAG, "Handler: applying UI mode");
                    applyScenarioNow(SCENARIO_UI);
                    break;
                case MSG_SET_VIDEO:
                    Log.v(TAG, "Handler: applying Video mode");
                    applyScenarioNow(SCENARIO_VIDEO);
                    break;
                case MSG_SET_CAMERA:
                    Log.v(TAG, "Handler: applying Camera mode");
                    applyScenarioNow(SCENARIO_CAMERA);
                    break;
                case MSG_SET_GALLERY:
                    Log.v(TAG, "Handler: applying Gallery mode");
                    applyScenarioNow(SCENARIO_GALLERY);
                    break;
                case MSG_SET_NAVIGATION:
                    Log.v(TAG, "Handler: applying Navigation mode");
                    applyScenarioNow(SCENARIO_NAVIGATION);
                    break;
                case MSG_SET_VCALL:
                    Log.v(TAG, "Handler: applying Video-Call mode");
                    applyScenarioNow(SCENARIO_VCALL);
                    break;
            }
        }
    }
}
