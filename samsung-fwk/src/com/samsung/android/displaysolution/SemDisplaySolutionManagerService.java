/*
 * Copyright (C) 2025 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package com.samsung.android.displaysolution;

import android.content.Context;
import android.content.res.Resources;
import android.util.Slog;

public final class SemDisplaySolutionManagerService extends ISemDisplaySolutionManager.Stub {
    private static final String TAG = "SemDisplaySolutionManagerService";

    private final int[] mBacklight;
    private final float[] mNits;
    private final float mGamma;

    public SemDisplaySolutionManagerService(Context context) {
        final Resources res = context.getResources();

        mBacklight = res.getIntArray(com.android.internal.R.array.config_screenBrightnessBacklight);
        mNits = loadFloatArray(res, com.android.internal.R.array.config_screenBrightnessNits);

        if (mBacklight.length != mNits.length) {
            throw new IllegalStateException("Brightness arrays mismatch: backlight="
                    + mBacklight.length + " nits=" + mNits.length);
        }

        mGamma = 2.2f;
        Slog.d(TAG, "Initialized: size=" + mNits.length + " gamma=" + mGamma);
    }

    @Override
    public float getFingerPrintBacklightValue(int brightnessNits) {
        final float n = (float) brightnessNits;
        for (int i = 0; i < mNits.length; i++) {
            if (n <= mNits[i]) {
                return (float) mBacklight[i];
            }
        }
        return -1.0f;
    }

    @Override
    public float getAlphaMaskLevel(float currentIdxF, float targetIdxF, float brCtrl) {
        final int cur = (int) currentIdxF;
        final int tgt = (int) targetIdxF;

        final float currentNits = (cur >= 0 && cur < mNits.length) ? mNits[cur] : -1f;
        final float targetNits  = (tgt >= 0 && tgt < mNits.length) ? mNits[tgt] : -1f;

        if (currentNits <= 0f || targetNits <= 0f || brCtrl <= 0f) {
            return 0f;
        }

        final double ratio = (currentNits * brCtrl) / targetNits;
        final double pow = Math.pow(ratio, 1.0d / mGamma);
        
        return (float) Math.max(0.0, Math.min(1.0, 1.0d - pow));
    }

    private static float[] loadFloatArray(Resources res, int arrayResId) {
        final String[] strs = res.getStringArray(arrayResId);
        final float[] out = new float[strs.length];
        for (int i = 0; i < strs.length; i++) {
            try {
                out[i] = Float.parseFloat(strs[i].trim());
            } catch (NumberFormatException e) {
                out[i] = 0f;
                Slog.w(TAG, "Invalid float in resource array at index " + i);
            }
        }
        return out;
    }
}
