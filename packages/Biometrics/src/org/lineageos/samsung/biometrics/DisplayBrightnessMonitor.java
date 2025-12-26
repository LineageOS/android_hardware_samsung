/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.samsung.biometrics;

import android.os.Build;
import android.os.FileObserver;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;

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
    private final FileObserver mBrightnessFileObserver;

    private final List<OnBrightnessListener> mListeners = new CopyOnWriteArrayList<>();

    private volatile int mCurrentBrightness = -1;

    public DisplayBrightnessMonitor(Looper looper) {
        mHandler = new Handler(looper);

        if ("qcom".equals(Build.HARDWARE)) {
            mBrightnessFilePath = "/sys/class/backlight/panel0-backlight/brightness";
        } else {
            mBrightnessFilePath = "/sys/class/lcd/panel/device/backlight/panel/brightness";
        }

        mBrightnessFileObserver = new BrightnessFileObserver(new File(mBrightnessFilePath));
    }

    private final class BrightnessFileObserver extends FileObserver {
        BrightnessFileObserver(File file) {
            super(file);
        }

        @Override
        public void onEvent(int event, String path) {
            int value = getBrightnessFromFile();
            if (value < 0) return;
            postBrightnessChanged(value);
        }
    }

    private void postBrightnessChanged(int brightness) {
        mHandler.removeCallbacksAndMessages(null);
        mHandler.post(() -> handleDisplayBrightnessChanged(brightness));
    }

    public int getBrightnessFromFile() {
        byte[] data;

        try {
            data = Utils.readFile(new File(mBrightnessFilePath));
        } catch (IOException e) {
            Log.w(TAG, "Failed to read brightness file", e);
            return -1;
        }

        try {
            int raw = Integer.parseInt(new String(data, StandardCharsets.UTF_8).trim());
            /*
             * Samsung logic (implement later if deemed necessary):
             * return "4".equals(Utils.Config.FEATURE_CONFIG_CONTROL_AUTO_BRIGHTNESS) ?
             *         (raw / 100) : raw;
             */
            return raw;
        } catch (NumberFormatException e) {
            Log.w(TAG, "getBrightnessFromFile: " + e.getMessage());
            return -1;
        } catch (Throwable t) {
            Log.w(TAG, "getBrightnessFromFile: " + t.getMessage());
            return -1;
        }
    }

    public void observe(boolean enable) {
        if (!enable) {
            mBrightnessFileObserver.stopWatching();
            return;
        }

        mBrightnessFileObserver.startWatching();

        int now = getBrightnessFromFile();
        if (now >= 0) {
            mCurrentBrightness = now;
        }
    }

    private void handleDisplayBrightnessChanged(int newValue) {
        if (mCurrentBrightness == newValue) return;

        mCurrentBrightness = newValue;

        for (OnBrightnessListener l : mListeners) {
            try {
                l.onBrightnessChanged(mCurrentBrightness);
            } catch (Throwable t) {
                Log.w(TAG, "Listener threw: " + t.getMessage());
            }
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
            mHandler.removeCallbacksAndMessages(null);
            observe(false);
        }
    }

    public int getCurrentBrightness() {
        return mCurrentBrightness;
    }

    public String getBrightnessFilePath() {
        return mBrightnessFilePath;
    }
}
