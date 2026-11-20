/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.biometricservice;

import android.content.Context;
import android.database.ContentObserver;
import android.hardware.display.BrightnessInfo;
import android.hardware.display.DisplayManager;
import android.os.Build;
import android.os.FileObserver;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.util.Log;
import android.view.Display;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;

/**
 * Monitors live panel display brightness and notifies listeners on the main looper.
 *
 * <p>Primary path watches the panel backlight sysfs node via {@link FileObserver} so dimming alpha
 * tracks brightness slider drags and auto-brightness ramps in real time (where AOSP suppresses or
 * sends target-only {@link DisplayManager#EVENT_TYPE_DISPLAY_BRIGHTNESS} callbacks). Falls back to
 * {@link Display#getBrightnessInfo()} and {@link DisplayManager} when sysfs is inaccessible.
 */
public final class DisplayBrightnessMonitor {
    private static final String TAG = "SB_BrightnessMonitor";

    public interface OnBrightnessListener {
        void onBrightnessChanged(int brightness);
    }

    private static final class InstanceHolder {
        private static final DisplayBrightnessMonitor INSTANCE =
                new DisplayBrightnessMonitor(Looper.getMainLooper());
    }

    public static DisplayBrightnessMonitor getInstance() {
        return InstanceHolder.INSTANCE;
    }

    private final Handler mHandler;
    private final String mBrightnessFilePath;
    private final String mMaxBrightnessFilePath;
    private final List<OnBrightnessListener> mListeners = new CopyOnWriteArrayList<>();
    private final Runnable mDispatchRunnable = this::dispatchPendingBrightness;

    private Context mContext;
    private DisplayManager mDisplayManager;
    private ContentObserver mContentObserver;
    private FileObserver mFileObserver;
    private boolean mFileObserverWatching;
    private boolean mDisplayListenerRegistered;

    private volatile float mMaxBacklight = 255.0f;
    private volatile float mSysfsMaxBacklight = -1.0f;
    private volatile float mCurrentBrightnessFloat = -1.0f;
    private volatile int mCurrentBrightness = -1;
    private volatile int mPendingBrightness = -1;

    private final DisplayManager.DisplayListener mDisplayListener =
            new DisplayManager.DisplayListener() {
                @Override
                public void onDisplayAdded(int displayId) {}

                @Override
                public void onDisplayRemoved(int displayId) {}

                @Override
                public void onDisplayChanged(int displayId) {
                    if (displayId == Display.DEFAULT_DISPLAY) {
                        refreshBrightness();
                    }
                }
            };

    public DisplayBrightnessMonitor(Looper looper) {
        mHandler = new Handler(looper);
        if ("qcom".equals(Build.HARDWARE)) {
            mBrightnessFilePath = "/sys/class/backlight/panel0-backlight/brightness";
            mMaxBrightnessFilePath = "/sys/class/backlight/panel0-backlight/max_brightness";
        } else {
            mBrightnessFilePath = "/sys/class/lcd/panel/device/backlight/panel/brightness";
            mMaxBrightnessFilePath = "/sys/class/lcd/panel/device/backlight/panel/max_brightness";
        }
    }

    // ---- Lifecycle & listeners ----

    public void init(Context context) {
        if (mContext != null || context == null) return;
        mContext = context.getApplicationContext();
        mDisplayManager = mContext.getSystemService(DisplayManager.class);

        readSysfsMaxBrightness();

        try {
            File brightnessFile = new File(mBrightnessFilePath);
            mFileObserver = new FileObserver(brightnessFile, FileObserver.MODIFY) {
                @Override
                public void onEvent(int event, String path) {
                    int mapped = readSysfsBrightness();
                    if (mapped >= 0) {
                        postBrightnessChanged(mapped);
                    }
                }
            };
        } catch (RuntimeException e) {
            Log.w(TAG, "Failed to create brightness FileObserver for " + mBrightnessFilePath, e);
            mFileObserver = null;
        }

        mContentObserver = new ContentObserver(mHandler) {
            @Override
            public void onChange(boolean selfChange) {
                refreshBrightness();
            }
        };

        try {
            mContext.getContentResolver().registerContentObserver(
                    Settings.System.getUriFor(Settings.System.SCREEN_BRIGHTNESS),
                    false,
                    mContentObserver);
            mContext.getContentResolver().registerContentObserver(
                    Settings.System.getUriFor("screen_brightness_float"),
                    false,
                    mContentObserver);
        } catch (Exception e) {
            Log.w(TAG, "Failed to register brightness content observer", e);
        }

        refreshBrightness();
    }

    public void setMaxBacklight(float maxBacklight) {
        if (maxBacklight > 0.0f && mMaxBacklight != maxBacklight) {
            mMaxBacklight = maxBacklight;
            refreshBrightness();
        }
    }

    public void registerListener(OnBrightnessListener listener) {
        if (listener == null) return;
        if (mListeners.contains(listener)) return;

        mListeners.add(listener);
        if (mListeners.size() == 1) {
            observe(true);
        }
    }

    public void unregisterListener(OnBrightnessListener listener) {
        if (listener == null) return;
        if (!mListeners.contains(listener)) return;

        mListeners.remove(listener);
        if (mListeners.isEmpty()) {
            mHandler.removeCallbacks(mDispatchRunnable);
            observe(false);
        }
    }

    private void observe(boolean enable) {
        if (enable) {
            if (mFileObserver != null && !mFileObserverWatching) {
                try {
                    mFileObserver.startWatching();
                    mFileObserverWatching = true;
                } catch (RuntimeException e) {
                    Log.w(TAG, "Failed to start brightness FileObserver", e);
                }
            }
            if (mDisplayManager != null && !mDisplayListenerRegistered) {
                try {
                    mDisplayManager.registerDisplayListener(
                            mDisplayListener, mHandler,
                            DisplayManager.EVENT_TYPE_DISPLAY_BRIGHTNESS);
                    mDisplayListenerRegistered = true;
                } catch (Exception e) {
                    Log.w(TAG, "Failed to register DisplayManager brightness listener", e);
                }
            }
            refreshBrightness();
        } else {
            if (mFileObserver != null && mFileObserverWatching) {
                try {
                    mFileObserver.stopWatching();
                } catch (RuntimeException e) {
                    Log.w(TAG, "Failed to stop brightness FileObserver", e);
                }
                mFileObserverWatching = false;
            }
            if (mDisplayManager != null && mDisplayListenerRegistered) {
                try {
                    mDisplayManager.unregisterDisplayListener(mDisplayListener);
                } catch (Exception e) {
                    Log.w(TAG, "Failed to unregister DisplayManager brightness listener", e);
                }
                mDisplayListenerRegistered = false;
            }
        }
    }

    // ---- Brightness dispatch ----

    public void refreshBrightness() {
        int mapped = queryBrightness();
        if (mapped >= 0) {
            postBrightnessChanged(mapped);
        }
    }

    public void postBrightnessChanged(int brightness) {
        mPendingBrightness = brightness;
        mHandler.removeCallbacks(mDispatchRunnable);
        mHandler.post(mDispatchRunnable);
    }

    private void dispatchPendingBrightness() {
        int newValue = mPendingBrightness;
        if (newValue < 0 || mCurrentBrightness == newValue) return;
        mCurrentBrightness = newValue;

        for (OnBrightnessListener l : mListeners) {
            try {
                l.onBrightnessChanged(mCurrentBrightness);
            } catch (Throwable t) {
                Log.w(TAG, "Listener threw: " + t.getMessage());
            }
        }
    }

    // ---- Brightness queries ----

    /**
     * Queries the current backlight level mapped onto {@code [0..mMaxBacklight]}, reading sysfs
     * first and falling back to {@link Display#getBrightnessInfo()} / {@link DisplayManager}.
     */
    private int queryBrightness() {
        int sysfsVal = readSysfsBrightness();
        if (sysfsVal >= 0) {
            return sysfsVal;
        }

        if (mDisplayManager != null) {
            try {
                Display display = mDisplayManager.getDisplay(Display.DEFAULT_DISPLAY);
                if (display != null) {
                    BrightnessInfo info = display.getBrightnessInfo();
                    if (info != null) {
                        float norm = (!Float.isNaN(info.adjustedBrightness)
                                && info.adjustedBrightness >= 0.0f)
                                ? info.adjustedBrightness
                                : info.brightness;
                        if (!Float.isNaN(norm) && norm >= 0.0f) {
                            float clamped = Math.min(1.0f, norm);
                            mCurrentBrightnessFloat = clamped;
                            return Math.round(clamped * mMaxBacklight);
                        }
                    }
                }
            } catch (RuntimeException e) {
                Log.w(TAG, "Display.getBrightnessInfo failed", e);
            }

            try {
                float norm = mDisplayManager.getBrightness(Display.DEFAULT_DISPLAY);
                if (!Float.isNaN(norm) && norm >= 0.0f) {
                    float clamped = Math.min(1.0f, norm);
                    mCurrentBrightnessFloat = clamped;
                    return Math.round(clamped * mMaxBacklight);
                }
            } catch (RuntimeException e) {
                Log.w(TAG, "DisplayManager.getBrightness failed", e);
            }
        }

        return -1;
    }

    private void readSysfsMaxBrightness() {
        int max = readIntFromFile(mMaxBrightnessFilePath);
        if (max > 0) {
            mSysfsMaxBacklight = max;
        }
    }

    private int readSysfsBrightness() {
        int raw = getBrightnessFromFile();
        if (raw < 0) return -1;

        if (mSysfsMaxBacklight <= 0.0f) {
            readSysfsMaxBrightness();
        }

        float sysfsMax = mSysfsMaxBacklight;
        if (sysfsMax > 0.0f) {
            float norm = Math.max(0.0f, Math.min(1.0f, raw / sysfsMax));
            mCurrentBrightnessFloat = norm;
            if (Math.abs(sysfsMax - mMaxBacklight) > 0.5f) {
                return Math.round(norm * mMaxBacklight);
            }
            return raw;
        }

        mCurrentBrightnessFloat = Math.max(0.0f, Math.min(1.0f, raw / Math.max(1.0f, mMaxBacklight)));
        return raw;
    }

    public int getCurrentBrightness() {
        int now = queryBrightness();
        if (now >= 0) {
            mCurrentBrightness = now;
        }
        return mCurrentBrightness;
    }

    public float getCurrentBrightnessFloat() {
        queryBrightness();
        return mCurrentBrightnessFloat;
    }

    public int getBrightnessFromFile() {
        return readIntFromFile(mBrightnessFilePath);
    }

    private int readIntFromFile(String path) {
        byte[] data;
        try {
            data = Utils.readFile(new File(path));
        } catch (IOException e) {
            return -1;
        }
        if (data == null || data.length == 0) return -1;

        try {
            return Integer.parseInt(new String(data, StandardCharsets.UTF_8).trim());
        } catch (NumberFormatException e) {
            Log.w(TAG, "readIntFromFile(" + path + "): " + e.getMessage());
            return -1;
        }
    }

    public String getBrightnessFilePath() {
        return mBrightnessFilePath;
    }
}
