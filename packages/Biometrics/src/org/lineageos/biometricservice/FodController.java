/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.biometricservice;

import android.app.KeyguardManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.database.ContentObserver;
import android.graphics.Rect;
import android.hardware.biometrics.BiometricStateListener;
import android.hardware.display.DisplayManager;
import android.net.Uri;
import android.os.Handler;
import android.os.PowerManager;
import android.os.UserHandle;
import android.provider.Settings;
import android.util.Log;
import android.view.Display;

/**
 * Manages touchscreen FoD (fingerprint-on-display) gesture modes and touch event forwarding for
 * both optical and ultrasonic sensors via {@link SehFingerprintClient}.
 */
public class FodController implements SehFingerprintClient.Listener {
    private static final String TAG = "SB_FodController";

    /** Duration of the AoD fast-mode window after a touch before reverting to strict mode. */
    private static final long AOD_FAST_TIMEOUT_MS = 5000;

    /** Settle delay after SCREEN_OFF before evaluating display state (allows DOZE to engage). */
    private static final long SCREEN_OFF_SETTLE_MS = 500;

    private static final String SETTING_SCREEN_OFF_UDFPS = "screen_off_udfps_enabled";

    /**
     * Broadcast action for manually triggering an AoD gesture during testing:
     * {@code adb shell am broadcast -a org.lineageos.biometrics.action.FINGERPRINT_GESTURE}
     */
    public static final String ACTION_FINGERPRINT_GESTURE =
            "org.lineageos.biometrics.action.FINGERPRINT_GESTURE";

    /** Unified listener for TSP FoD touch events, AoD transitions, and HBM updates. */
    public interface FodListener {
        void onFingerDown(int x, int y);
        void onFingerUp();
        default void onAodStateChanged(boolean armed, boolean fast) {}
        default void onWokeFromAod() {}
        default void onOpticalCaptureStarted() {}
        default void onOpticalCaptureFinished() {}
        default void onHbmChanged(boolean enabled) {}
        default void onHalConnected() {}
    }

    private enum FodState { DISABLED, SCREEN_ON_FAST, AOD_STRICT, AOD_FAST }

    private final Context mContext;
    private final SehFingerprintClient mClient;
    private final Handler mHandler;
    private final DisplayManager mDisplayManager;
    private final PowerManager mPowerManager;
    private final PowerManager.WakeLock mWakeLock;
    private final KeyguardManager mKeyguardManager;

    private FodState mCurrentState = FodState.DISABLED;
    private boolean mScreenOffUdfpsEnabled;
    private int mBiometricState = BiometricStateListener.STATE_IDLE;
    private boolean mRunning;

    private Rect mFodRect;
    private boolean mLastIconVisible = false;

    private ContentObserver mUdfpsObserver;
    private FodListener mFodListener;

    private final Runnable mEvaluateRunnable = this::evaluateState;

    private final Runnable mAodFastTimeout = () -> {
        if (mCurrentState == FodState.AOD_FAST) {
            Log.d(TAG, "AoD fast mode timeout expired, reverting to strict");
            transitionToState(FodState.AOD_STRICT);
        }
    };

    private final BroadcastReceiver mScreenReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent == null || intent.getAction() == null)
                return;
            switch (intent.getAction()) {
                case Intent.ACTION_SCREEN_ON:
                case Intent.ACTION_USER_PRESENT:
                    mHandler.removeCallbacks(mEvaluateRunnable);
                    evaluateState();
                    break;
                case Intent.ACTION_SCREEN_OFF:
                    mHandler.removeCallbacks(mEvaluateRunnable);
                    mHandler.postDelayed(mEvaluateRunnable, SCREEN_OFF_SETTLE_MS);
                    break;
            }
        }
    };

    private final BroadcastReceiver mFpGestureReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent != null && ACTION_FINGERPRINT_GESTURE.equals(intent.getAction())) {
                onFingerprintGesture();
            }
        }
    };

    private final DisplayManager.DisplayListener mDisplayListener =
            new DisplayManager.DisplayListener() {
                @Override
                public void onDisplayChanged(int displayId) {
                    if (displayId == Display.DEFAULT_DISPLAY)
                        evaluateState();
                }

                @Override
                public void onDisplayAdded(int displayId) {}

                @Override
                public void onDisplayRemoved(int displayId) {}
            };

    private final KeyguardManager.KeyguardLockedStateListener mKeyguardListener =
            isKeyguardLocked -> {
        Log.d(TAG, "Keyguard locked: " + isKeyguardLocked);
        evaluateState();
    };

    public FodController(Context context, SehFingerprintClient client, Handler handler) {
        mContext = context;
        mClient = client;
        mHandler = handler;
        mDisplayManager = context.getSystemService(DisplayManager.class);
        mKeyguardManager = context.getSystemService(KeyguardManager.class);
        mPowerManager = context.getSystemService(PowerManager.class);
        if (mPowerManager != null) {
            mWakeLock = mPowerManager.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, TAG + ":AodFast");
            mWakeLock.setReferenceCounted(false);
        } else {
            mWakeLock = null;
        }
    }

    // ---- Listeners & AoD ----

    public SehFingerprintClient getClient() { return mClient; }

    /** Registers the FoD listener and immediately reports current state. */
    public void setFodListener(FodListener listener) {
        mFodListener = listener;
        notifyAod();
    }

    /**
     * An AoD press was detected: start the AOD_FAST window, or restart
     * its 5 s timer if it is already running.
     */
    public void onAodPress() { onFingerprintGesture(); }

    private void notifyAod() {
        FodListener l = mFodListener;
        if (l == null)
            return;
        final boolean armed =
                mCurrentState == FodState.AOD_STRICT || mCurrentState == FodState.AOD_FAST;
        l.onAodStateChanged(armed, mCurrentState == FodState.AOD_FAST);
    }

    // ---- SehFingerprintClient.Listener ----

    @Override
    public void onConnected() {
        if (!mRunning)
            return;
        applyConfig();
        sendFodState(mCurrentState);
        updateIconVisibility();
        if (mFodListener != null) {
            mFodListener.onHalConnected();
        }
    }

    public void onAuthenticationSucceeded() {
        Log.i(TAG, "onAuthenticationSucceeded: immediately hiding FoD icon");
        if (mLastIconVisible) {
            mLastIconVisible = false;
            mClient.setFodIconVisible(false);
        }
    }

    @Override
    public void onFingerDown(int x, int y) {
        if (!mRunning)
            return;
        Log.d(TAG, "onFingerDown (" + x + ", " + y + ") in state=" + mCurrentState);
        if (mCurrentState == FodState.AOD_STRICT || mCurrentState == FodState.AOD_FAST) {
            onAodPress();
        }
        if (mFodListener != null) {
            mFodListener.onFingerDown(x, y);
        }
    }

    @Override
    public void onFingerUp() {
        if (!mRunning)
            return;
        Log.d(TAG, "onFingerUp");
        if (mFodListener != null) {
            mFodListener.onFingerUp();
        }
    }

    @Override
    public void onOpticalCaptureStarted() {
        if (!mRunning)
            return;
        Log.d(TAG, "onOpticalCaptureStarted in state=" + mCurrentState);
        if (mCurrentState == FodState.AOD_STRICT || mCurrentState == FodState.AOD_FAST) {
            onAodPress();
        }
        if (mFodListener != null) {
            mFodListener.onOpticalCaptureStarted();
        }
    }

    @Override
    public void onOpticalCaptureFinished() {
        if (!mRunning)
            return;
        Log.d(TAG, "onOpticalCaptureFinished");
        if (mFodListener != null) {
            mFodListener.onOpticalCaptureFinished();
        }
    }

    @Override
    public void onHbmChanged(boolean enabled) {
        if (!mRunning)
            return;
        if (mFodListener != null) {
            mFodListener.onHbmChanged(enabled);
        }
    }

    // ---- Lifecycle ----

    public void configure() {
        if (mRunning && mClient.isConnected())
            applyConfig();
    }

    public void configure(boolean ignored) { configure(); }

    public void configure(Rect fodRect) {
        mFodRect = fodRect;
        if (mRunning && mClient.isConnected())
            applyConfig();
    }

    public void configure(Rect fodRect, boolean ignored) { configure(fodRect); }

    public void configure(Rect fodRect, boolean ignored1, boolean ignored2) { configure(fodRect); }

    public void start() {
        if (mRunning)
            return;
        mRunning = true;

        IntentFilter screenFilter = new IntentFilter();
        screenFilter.addAction(Intent.ACTION_SCREEN_ON);
        screenFilter.addAction(Intent.ACTION_SCREEN_OFF);
        screenFilter.addAction(Intent.ACTION_USER_PRESENT);
        mContext.registerReceiver(mScreenReceiver, screenFilter, Context.RECEIVER_NOT_EXPORTED);

        mContext.registerReceiver(mFpGestureReceiver, new IntentFilter(ACTION_FINGERPRINT_GESTURE),
                Context.RECEIVER_EXPORTED);

        mDisplayManager.registerDisplayListener(mDisplayListener, mHandler);

        if (mKeyguardManager != null) {
            mKeyguardManager.addKeyguardLockedStateListener(
                    mContext.getMainExecutor(), mKeyguardListener);
        }

        Uri udfpsUri = Settings.Secure.getUriFor(SETTING_SCREEN_OFF_UDFPS);
        mUdfpsObserver = new ContentObserver(mHandler) {
            @Override
            public void onChange(boolean selfChange) {
                onUdfpsSettingChanged();
            }
        };
        mContext.getContentResolver().registerContentObserver(
                udfpsUri, false, mUdfpsObserver, UserHandle.USER_ALL);

        mScreenOffUdfpsEnabled = isScreenOffUdfpsEnabled();

        mClient.setListener(this);
        mClient.connect();

        evaluateState();
        Log.i(TAG, "started");
    }

    public void stop() {
        if (!mRunning)
            return;
        mRunning = false;

        mHandler.removeCallbacks(mAodFastTimeout);
        mHandler.removeCallbacks(mEvaluateRunnable);

        if (mWakeLock != null && mWakeLock.isHeld()) {
            mWakeLock.release();
        }

        mCurrentState = FodState.DISABLED;
        sendFodState(FodState.DISABLED);
        notifyAod();

        mLastIconVisible = false;
        mClient.setFodIconVisible(false);

        try {
            mContext.unregisterReceiver(mScreenReceiver);
        } catch (IllegalArgumentException ignored) {
        }
        try {
            mContext.unregisterReceiver(mFpGestureReceiver);
        } catch (IllegalArgumentException ignored) {
        }

        mDisplayManager.unregisterDisplayListener(mDisplayListener);

        if (mKeyguardManager != null) {
            mKeyguardManager.removeKeyguardLockedStateListener(mKeyguardListener);
        }
        if (mUdfpsObserver != null) {
            mContext.getContentResolver().unregisterContentObserver(mUdfpsObserver);
            mUdfpsObserver = null;
        }

        mClient.disconnect();
        Log.i(TAG, "stopped");
    }

    public void onBiometricStateChanged(int newState) {
        Log.i(TAG, "Biometric state changed: " + newState);
        mBiometricState = newState;
        evaluateState();
    }

    private void onFingerprintGesture() {
        if (mCurrentState != FodState.AOD_STRICT && mCurrentState != FodState.AOD_FAST) {
            Log.d(TAG, "FP gesture ignored (state " + mCurrentState + ")");
            return;
        }
        Log.i(TAG, "AoD press — AOD_FAST window (re)started");
        mHandler.removeCallbacks(mAodFastTimeout);

        if (mWakeLock != null) {
            mWakeLock.acquire(AOD_FAST_TIMEOUT_MS);
        }

        transitionToState(FodState.AOD_FAST);
        mHandler.postDelayed(mAodFastTimeout, AOD_FAST_TIMEOUT_MS);
    }

    private void onUdfpsSettingChanged() {
        boolean enabled = isScreenOffUdfpsEnabled();
        Log.i(TAG, "Screen-Off UDFPS setting changed: " + enabled);
        mScreenOffUdfpsEnabled = enabled;

        if (!enabled && (mCurrentState == FodState.AOD_STRICT || mCurrentState == FodState.AOD_FAST)
                && getDisplayState() == Display.STATE_OFF) {
            transitionToState(FodState.DISABLED);
        } else {
            evaluateState();
        }
    }

    // ---- State machine ----

    private void evaluateState() {
        if (!mRunning)
            return;

        updateIconVisibility();

        FodState desired = computeDesiredState();
        if (desired == mCurrentState)
            return;

        // The 5 s timeout, not a re-evaluation, moves AOD_FAST back to AOD_STRICT.
        if (mCurrentState == FodState.AOD_FAST && desired == FodState.AOD_STRICT)
            return;

        transitionToState(desired);
    }

    private boolean computeDesiredIconVisible() {
        if (mBiometricState == BiometricStateListener.STATE_IDLE) {
            return false;
        }

        if (mCurrentState == FodState.AOD_FAST) {
            return true;
        }

        int displayState = getDisplayState();
        switch (displayState) {
            case Display.STATE_ON:
            case Display.STATE_VR:
            case Display.STATE_DOZE:
            case Display.STATE_DOZE_SUSPEND:
                // AOSP renders the UDFPS icon in both screen-on and AoD states.
                return true;

            case Display.STATE_OFF:
            case Display.STATE_UNKNOWN:
            default:
                // Panel is off; no icon is visible even when Screen-Off UDFPS is armed.
                return false;
        }
    }

    private void updateIconVisibility() {
        if (!mRunning || !mClient.isConnected())
            return;

        boolean visible = computeDesiredIconVisible();
        if (mLastIconVisible != visible) {
            mLastIconVisible = visible;
            Log.i(TAG, "updateIconVisibility: " + visible);
            mClient.setFodIconVisible(visible);
        }
    }

    private FodState computeDesiredState() {
        boolean interactive = mPowerManager == null || mPowerManager.isInteractive();
        switch (getDisplayState()) {
            case Display.STATE_ON:
            case Display.STATE_VR:
                if (mBiometricState == BiometricStateListener.STATE_IDLE) {
                    return FodState.DISABLED;
                }
                return interactive ? FodState.SCREEN_ON_FAST : FodState.AOD_STRICT;

            case Display.STATE_DOZE:
            case Display.STATE_DOZE_SUSPEND:
                return mBiometricState != BiometricStateListener.STATE_IDLE ? FodState.AOD_STRICT
                                                                            : FodState.DISABLED;

            case Display.STATE_OFF:
                return (mScreenOffUdfpsEnabled
                               && mBiometricState != BiometricStateListener.STATE_IDLE)
                        ? FodState.AOD_STRICT
                        : FodState.DISABLED;

            case Display.STATE_UNKNOWN:
            default:
                return FodState.DISABLED;
        }
    }

    private void transitionToState(FodState newState) {
        FodState oldState = mCurrentState;
        mCurrentState = newState;

        if (oldState == FodState.AOD_FAST && newState != FodState.AOD_FAST) {
            mHandler.removeCallbacks(mAodFastTimeout);
            if (mWakeLock != null && mWakeLock.isHeld()) {
                mWakeLock.release();
            }
        }

        Log.i(TAG, "FoD: " + oldState + " -> " + newState);
        sendFodState(newState);

        if (newState == FodState.SCREEN_ON_FAST
                && (oldState == FodState.AOD_STRICT || oldState == FodState.AOD_FAST)
                && mFodListener != null) {
            mFodListener.onWokeFromAod();
        }
        notifyAod();
        updateIconVisibility();
    }

    // ---- HAL & system helpers ----

    private void sendFodState(FodState state) {
        int mode;
        switch (state) {
            case SCREEN_ON_FAST:
                mode = 1;
                break;
            case AOD_STRICT:
                mode = 2;
                break;
            case AOD_FAST:
                mode = 3;
                break;
            case DISABLED:
            default:
                mode = 0;
                break;
        }
        mClient.setFodEnable(mode);
    }

    private void applyConfig() {
        if (mFodRect != null) {
            mClient.setFodRect(mFodRect);
        }
        updateIconVisibility();
    }

    private int getDisplayState() {
        Display display = mDisplayManager.getDisplay(Display.DEFAULT_DISPLAY);
        return display != null ? display.getState() : Display.STATE_UNKNOWN;
    }

    private boolean isScreenOffUdfpsEnabled() {
        return Settings.Secure.getIntForUser(mContext.getContentResolver(),
                       SETTING_SCREEN_OFF_UDFPS, 0, UserHandle.USER_CURRENT)
                != 0;
    }
}
