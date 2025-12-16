/*
 * Copyright (C) 2025 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.samsung.biometrics;

import android.annotation.NonNull;
import android.content.Context;
import android.graphics.Rect;
import android.hardware.biometrics.SensorLocationInternal;
import android.hardware.fingerprint.FingerprintManager;
import android.hardware.fingerprint.FingerprintSensorPropertiesInternal;
import android.util.Log;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.util.List;

public class SemFodRectCalculator {
    private static final String TAG = "SemFodRectCalculator";

    public static final String TSP_CMD_PATH = "/sys/class/sec/tsp/cmd";

    @NonNull
    private final Context mContext;

    @NonNull
    private Rect mFodRect = new Rect();

    public SemFodRectCalculator(@NonNull Context context) {
        mContext = context;
    }

    public boolean writeFodRect() {
        final Rect rect = getUdfpsRectFromFramework();
        if (rect == null || rect.isEmpty()) {
            Log.w(TAG, "No valid UDFPS rect available (rect=" + rect + ")");
            return false;
        }

        mFodRect = new Rect(rect);

        final String cmd = buildTspFodRectCommand(mFodRect);
        try {
            Utils.writeFile(new File(TSP_CMD_PATH), cmd.getBytes(StandardCharsets.UTF_8));
            Log.i(TAG, "Wrote FOD rect to TSP: " + cmd.trim());
            return true;
        } catch (Throwable t) {
            Log.e(TAG, "Failed writing FOD rect to " + TSP_CMD_PATH, t);
            return false;
        }
    }

    @NonNull
    private static String buildTspFodRectCommand(@NonNull Rect r) {
        return "set_fod_rect," + r.left + "," + r.top + "," + r.right + "," + r.bottom + "\n";
    }

    private Rect getUdfpsRectFromFramework() {
        final FingerprintManager fm = mContext.getSystemService(FingerprintManager.class);
        if (fm == null) {
            Log.w(TAG, "FingerprintManager is null");
            return null;
        }

        final List<FingerprintSensorPropertiesInternal> props;
        try {
            props = fm.getSensorPropertiesInternal();
        } catch (Throwable t) {
            Log.e(TAG, "getSensorPropertiesInternal() not available on this branch", t);
            return null;
        }

        if (props == null || props.isEmpty()) {
            Log.w(TAG, "No fingerprint sensor properties");
            return null;
        }

        for (FingerprintSensorPropertiesInternal p : props) {
            if (!p.isAnyUdfpsType()) continue;

            final List<SensorLocationInternal> locs = p.getAllLocations();
            if (locs == null || locs.isEmpty()) continue;

            final SensorLocationInternal loc = locs.get(0);
            if (loc == null) continue;

            final Rect r = loc.getRect();
            if (r != null && !r.isEmpty()) {
                Log.i(TAG, "Using UDFPS sensorId=" + p.sensorId + " rect=" + r);
                return r;
            }
        }

        Log.w(TAG, "No UDFPS sensor rect found in properties");
        return null;
    }

    @NonNull
    public Rect getLastFodRect() {
        return new Rect(mFodRect);
    }
}
