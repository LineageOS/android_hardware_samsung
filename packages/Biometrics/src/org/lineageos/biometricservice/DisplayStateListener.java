/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.biometricservice;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.display.DisplayManager;
import android.os.Handler;
import android.os.Looper;
import android.os.PowerManager;
import android.util.Log;
import android.view.Display;

/**
 * Monitors default display power states and interactivity via {@link
 * DisplayManager.DisplayListener},
 * {@link PowerManager#isInteractive()}, and screen on/off broadcasts.
 */
public class DisplayStateListener implements DisplayManager.DisplayListener {
    private static final String TAG = "SB_DisplayStateListener";

    public static final int STATE_UNKNOWN = Display.STATE_UNKNOWN;
    public static final int STATE_OFF = Display.STATE_OFF;
    public static final int STATE_ON = Display.STATE_ON;
    public static final int STATE_DOZE = Display.STATE_DOZE;
    public static final int STATE_DOZE_SUSPEND = Display.STATE_DOZE_SUSPEND;

    public interface OnDisplayStateChangedListener {
        void onDisplayStateChanged(int displayState);
    }

    private final Context mContext;
    private final DisplayManager mDisplayManager;
    private final PowerManager mPowerManager;
    private final Handler mMain = new Handler(Looper.getMainLooper());
    private final OnDisplayStateChangedListener mListener;

    private int mCurrentDisplayState = STATE_UNKNOWN;
    private boolean mCurrentInteractive;
    private boolean mRegistered;

    private final BroadcastReceiver mScreenReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent == null || intent.getAction() == null)
                return;
            Log.d(TAG, "Screen broadcast: " + intent.getAction());
            mMain.post(DisplayStateListener.this::updateState);
        }
    };

    public DisplayStateListener(Context context, OnDisplayStateChangedListener listener) {
        mContext = context;
        mListener = listener;
        mDisplayManager = context.getSystemService(DisplayManager.class);
        mPowerManager = context.getSystemService(PowerManager.class);
    }

    // ---- Lifecycle ----

    public synchronized void register() {
        if (mRegistered)
            return;
        mRegistered = true;
        mDisplayManager.registerDisplayListener(this, mMain);

        IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_SCREEN_ON);
        filter.addAction(Intent.ACTION_SCREEN_OFF);
        filter.addAction(Intent.ACTION_USER_PRESENT);
        mContext.registerReceiver(mScreenReceiver, filter, Context.RECEIVER_NOT_EXPORTED);

        updateState();
    }

    public synchronized void unregister() {
        if (!mRegistered)
            return;
        mRegistered = false;
        mDisplayManager.unregisterDisplayListener(this);
        try {
            mContext.unregisterReceiver(mScreenReceiver);
        } catch (IllegalArgumentException ignored) {
        }
    }

    // ---- DisplayManager.DisplayListener ----

    @Override
    public void onDisplayAdded(int displayId) {}

    @Override
    public void onDisplayRemoved(int displayId) {}

    @Override
    public void onDisplayChanged(int displayId) {
        if (displayId == Display.DEFAULT_DISPLAY) {
            updateState();
            DisplayBrightnessMonitor.getInstance().refreshBrightness();
        }
    }

    private void updateState() {
        int newState = queryDisplayState();
        boolean newInteractive = isInteractive();
        if (mCurrentDisplayState != newState || mCurrentInteractive != newInteractive) {
            Log.i(TAG,
                    "Display state changed: " + stateToString(mCurrentDisplayState) + " -> "
                            + stateToString(newState) + " (interactive=" + newInteractive + ")");
            mCurrentDisplayState = newState;
            mCurrentInteractive = newInteractive;
            if (mListener != null) {
                mListener.onDisplayStateChanged(mCurrentDisplayState);
            }
        }
    }

    // ---- State queries ----

    public int getDisplayState() {
        if (mCurrentDisplayState == STATE_UNKNOWN) {
            mCurrentDisplayState = queryDisplayState();
        }
        return mCurrentDisplayState;
    }

    public boolean isScreenOn() { return getDisplayState() == STATE_ON; }

    public boolean isDoze() {
        int state = getDisplayState();
        return state == STATE_DOZE || state == STATE_DOZE_SUSPEND;
    }

    public boolean isScreenOff() {
        int state = getDisplayState();
        return state == STATE_OFF || isDoze()
                || (mPowerManager != null && !mPowerManager.isInteractive());
    }

    public boolean isInteractive() {
        return mPowerManager != null && mPowerManager.isInteractive();
    }

    private int queryDisplayState() {
        Display display = mDisplayManager.getDisplay(Display.DEFAULT_DISPLAY);
        return display != null ? display.getState() : STATE_UNKNOWN;
    }

    public static String stateToString(int state) {
        switch (state) {
            case STATE_OFF:
                return "OFF";
            case STATE_ON:
                return "ON";
            case STATE_DOZE:
                return "DOZE";
            case STATE_DOZE_SUSPEND:
                return "DOZE_SUSPEND";
            case STATE_UNKNOWN:
                return "UNKNOWN";
            default:
                return "OTHER(" + state + ")";
        }
    }
}
