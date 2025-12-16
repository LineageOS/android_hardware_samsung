/*
 * Copyright (C) 2025 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.samsung.biometrics;

import android.util.Log;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.atomic.AtomicInteger;

public final class SemFodModeController {
    private static final String TAG = "SemFodModeController";

    // Display states
    public static final int DISPLAY_STATE_UNKNOWN    = 0;
    public static final int DISPLAY_STATE_LOCKSCREEN = 1;
    public static final int DISPLAY_STATE_NO_UI      = 2;
    public static final int DISPLAY_STATE_SCREENSAVER= 3;
    public static final int DISPLAY_STATE_AOD        = 4;

    // Biometric states
    public static final int STATE_IDLE         = 0;
    public static final int STATE_ENROLLING    = 1;
    public static final int STATE_KEYGUARD_AUTH= 2;
    public static final int STATE_BP_AUTH      = 3;
    public static final int STATE_AUTH_OTHER   = 4;

    // TSP sysfs
    public static final String TSP_CMD_PATH = "/sys/class/sec/tsp/cmd";

    // TSP keyword + params
    private static final String CMD_ENABLE     = "fod_enable,1,1,0\n"; // normal
    private static final String CMD_ENABLE_50  = "fod_enable,1,0,0\n"; // AoD delayed
    private static final String CMD_DISABLE    = "fod_enable,0,0,0\n";

    private final AtomicInteger mBiometricState = new AtomicInteger(STATE_IDLE);
    private final AtomicInteger mDisplayState   = new AtomicInteger(DISPLAY_STATE_UNKNOWN);

    private volatile String mLastCmd = null;

    public void onBiometricStateChanged(int newState) {
        mBiometricState.set(newState);
        updateTspFodMode();
    }

    public void onDisplayStateChanged(int newState) {
        mDisplayState.set(newState);
        updateTspFodMode();
    }

    private void updateTspFodMode() {
        final int bio = mBiometricState.get();
        final int disp = mDisplayState.get();

        final String desired = computeCommand(bio, disp);

        if (desired.equals(mLastCmd)) return;
        mLastCmd = desired;

        try {
            Utils.writeFile(new File(TSP_CMD_PATH), desired.getBytes(StandardCharsets.UTF_8));
            Log.i(TAG, "TSP cmd: " + desired.trim() + " (bio=" + bio + ", disp=" + disp + ")");
        } catch (Throwable t) {
            Log.e(TAG, "Failed to write TSP cmd: " + desired.trim(), t);
        }
    }

    private static String computeCommand(int biometricState, int displayState) {
        if (biometricState == STATE_IDLE) {
            return CMD_DISABLE;
        }

        final boolean active =
                biometricState == STATE_ENROLLING ||
                biometricState == STATE_KEYGUARD_AUTH ||
                biometricState == STATE_BP_AUTH ||
                biometricState == STATE_AUTH_OTHER;

        if (!active) {
            return CMD_DISABLE;
        }

        if (displayState == DISPLAY_STATE_AOD) {
            return CMD_ENABLE_50;
        }

        return CMD_ENABLE;
    }
}
