/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.biometricservice;

import android.content.Context;
import android.util.Log;

/**
 * Computes optical UDFPS dim-layer alpha values and backlight-to-nits mappings from the device's
 * panel luminance lookup table (LUT) and AOD brightness buckets.
 */
public class DisplaySolution {

    private static final String TAG = "SB_DisplaySolution";

    public static final float MAX_ALPHA = 0.93f;
    private static final float DEFAULT_GAMMA = 2.2f;
    private static final float FALLBACK_GAMMA = 1.85f;
    private static final float DEFAULT_UDFPS_BRIGHTNESS = 319.0f;
    private static final float DEFAULT_OPTICAL_NITS = 525.0f;
    private static final float DEFAULT_MAX_BACKLIGHT = 255.0f;

    private final float[] mBacklightLut;
    private final float[] mNitsLut;
    private final float mGamma;
    private final float mMaxBacklight;
    private final int[] mAodBrightnessValues;

    public DisplaySolution(Context context) {
        String[] blStrings = context.getResources().getStringArray(
                R.array.config_Screen_Brightness_Backlight_Value);
        String[] nitsStrings = context.getResources().getStringArray(
                R.array.config_Screen_Brightness_Nits_Value);
        String[] gammaStrings = context.getResources().getStringArray(
                R.array.config_Display_Solution_Gamma_Value);
        int[] aodValues = context.getResources().getIntArray(
                R.array.config_aodBrightnessValues);

        int lutLen = (blStrings != null && nitsStrings != null)
                ? Math.min(blStrings.length, nitsStrings.length) : 0;

        if (lutLen > 1) {
            mBacklightLut = new float[lutLen];
            mNitsLut = new float[lutLen];
            for (int i = 0; i < lutLen; i++) {
                mBacklightLut[i] = parseFloatOr(blStrings[i], i);
                mNitsLut[i] = parseFloatOr(nitsStrings[i], -1.0f);
            }
            float lastBl = mBacklightLut[lutLen - 1];
            mMaxBacklight = lastBl > 0.0f ? lastBl : DEFAULT_MAX_BACKLIGHT;
        } else {
            mBacklightLut = new float[0];
            mNitsLut = new float[0];
            mMaxBacklight = DEFAULT_MAX_BACKLIGHT;
        }

        if (gammaStrings != null && gammaStrings.length > 0) {
            float parsed = parseFloatOr(gammaStrings[0], DEFAULT_GAMMA);
            mGamma = parsed > 0.0f ? parsed : DEFAULT_GAMMA;
        } else {
            mGamma = DEFAULT_GAMMA;
        }

        mAodBrightnessValues = (aodValues != null && aodValues.length >= 2) ? aodValues : null;

        Log.i(TAG, "Initialized DisplaySolution: lutLen=" + mBacklightLut.length
                + " maxBacklight=" + mMaxBacklight + " gamma=" + mGamma
                + " aodBuckets=" + (mAodBrightnessValues != null ? mAodBrightnessValues.length : 0));
    }

    public float getMaxBacklight() {
        return mMaxBacklight;
    }

    // ---- Alpha calculation ----

    /**
     * Computes compensating dim-layer alpha in {@code [0.0f, MAX_ALPHA]} for the given current
     * backlight level and optical HBM target backlight level:
     * {@code alpha = 1 - ((currentNits * brCtrl) / targetNits) ^ (1 / gamma)}.
     * Falls back to a piecewise luminance approximation if the panel LUT is unavailable.
     */
    public float getAlphaMaskLevel(float currentBrightness, float fingerPrintBrightness,
                                   float brCtrl) {
        float alpha = calculateAlphaBlendingValue(currentBrightness, fingerPrintBrightness, brCtrl);
        return Math.min(MAX_ALPHA, Math.max(0.0f, alpha));
    }

    public float calculateAlphaBlendingValue(float currentBrightness, float fingerPrintBrightness,
                                             float brCtrl) {
        float currentNits = getNitsForBacklight(currentBrightness);
        float targetNits = getNitsForBacklight(fingerPrintBrightness);

        if (currentNits > 0.0f && targetNits > 0.0f) {
            float ratio = (currentNits * brCtrl) / targetNits;
            if (ratio >= 1.0f) {
                return 0.0f;
            }
            float alpha = (float) (1.0d - Math.pow(ratio, 1.0d / mGamma));
            Log.d(TAG, "LUT alpha: bl=" + currentBrightness + " (" + currentNits + " cd) "
                    + "fpBl=" + fingerPrintBrightness + " (" + targetNits + " cd) "
                    + "gamma=" + mGamma + " -> alpha=" + alpha);
            return alpha;
        }

        // Fallback piecewise luminance estimate when no panel LUT is configured.
        float b = Math.max(0.0f, currentBrightness);
        float linear = 1.644f * b;
        float corr;
        if (b <= 128.0f) {
            corr = 0.23f * b - 2.0f;
        } else if (b <= 255.0f) {
            corr = (255.0f - b) * 0.215f;
        } else {
            corr = 0.0f;
        }
        float estNit = Math.max(0.0f, linear - corr);
        float hbmNit = targetNits > 0.0f ? targetNits : DEFAULT_OPTICAL_NITS;
        if (estNit >= hbmNit) {
            return 0.0f;
        }
        float alpha = (float) (1.0d - Math.pow(estNit / hbmNit, 1.0d / FALLBACK_GAMMA));
        Log.d(TAG, "Fallback alpha: bl=" + b + " estNit=" + estNit + " hbmNit=" + hbmNit
                + " -> alpha=" + alpha);
        return alpha;
    }

    // ---- Luminance & AOD lookup ----

    /**
     * Piecewise-linear lookup of panel nits for a given backlight value in the LUT.
     * Returns {@code -1.0f} if the LUT is unconfigured.
     */
    public float getNitsForBacklight(float backlight) {
        int len = mBacklightLut.length;
        if (len < 2) {
            return -1.0f;
        }

        if (backlight <= mBacklightLut[0]) {
            return mNitsLut[0];
        }
        if (backlight >= mBacklightLut[len - 1]) {
            return mNitsLut[len - 1];
        }

        // Binary search for the bracketing interval [lo, hi]
        int lo = 0;
        int hi = len - 1;
        while (lo + 1 < hi) {
            int mid = (lo + hi) >>> 1;
            if (mBacklightLut[mid] <= backlight) {
                lo = mid;
            } else {
                hi = mid;
            }
        }

        float bl0 = mBacklightLut[lo];
        float bl1 = mBacklightLut[hi];
        float n0 = mNitsLut[lo];
        float n1 = mNitsLut[hi];
        if (bl1 <= bl0) {
            return n0;
        }
        float t = (backlight - bl0) / (bl1 - bl0);
        return n0 + t * (n1 - n0);
    }

    /**
     * Finds the lowest backlight value in the LUT whose luminance is at least {@code requiredNits}.
     */
    public float getFingerPrintBacklightValue(int requiredNits) {
        for (int i = 0; i < mNitsLut.length; i++) {
            if (mNitsLut[i] >= 0.0f && requiredNits <= Math.round(mNitsLut[i])) {
                Log.d(TAG, "getFingerPrintBacklightValue(" + requiredNits + " cd) -> "
                        + mBacklightLut[i]);
                return mBacklightLut[i];
            }
        }
        return -1.0f;
    }

    /**
     * Quantizes raw brightness into discrete AOD luminance buckets (2, 10, 30, 60 nits) using
     * {@code config_aodBrightnessValues} and returns the corresponding LUT backlight value.
     */
    public float getAodBacklightValue(float rawBrightness) {
        int nitBucket = 2;
        if (mAodBrightnessValues != null) {
            for (int idx = mAodBrightnessValues.length - 1; idx >= 0; idx--) {
                if (idx == 0) {
                    nitBucket = 2;
                    break;
                }
                int threshold = mAodBrightnessValues[idx];
                if (threshold > 0 && rawBrightness >= threshold) {
                    if (idx == 1) {
                        nitBucket = 10;
                    } else if (idx == 2) {
                        nitBucket = 30;
                    } else {
                        nitBucket = 60;
                    }
                    break;
                }
            }
        } else {
            nitBucket = rawBrightness > 90.0f ? 60 : 2;
        }

        float bl = getFingerPrintBacklightValue(nitBucket);
        return bl >= 0.0f ? bl : rawBrightness;
    }

    /**
     * Resolves the target optical UDFPS backlight level from the vendor HAL's target nits mapped
     * through the panel LUT, falling back to {@link #DEFAULT_OPTICAL_NITS} / {@link #DEFAULT_UDFPS_BRIGHTNESS}.
     */
    public float getUdfpsBrightness() {
        int targetNits = SehFingerprintClient.getTargetNits();
        if (targetNits > 0) {
            float bl = getFingerPrintBacklightValue(targetNits);
            if (bl > 0.0f) {
                Log.i(TAG, "Resolved UDFPS brightness from HAL (" + targetNits
                        + " nits) -> backlight " + bl);
                return bl;
            }
        }

        float defaultNitsBl = getFingerPrintBacklightValue(Math.round(DEFAULT_OPTICAL_NITS));
        if (defaultNitsBl > 0.0f) {
            Log.i(TAG, "Resolved UDFPS brightness from default 525 nits -> backlight "
                    + defaultNitsBl);
            return defaultNitsBl;
        }

        Log.i(TAG, "Using fallback UDFPS brightness: " + DEFAULT_UDFPS_BRIGHTNESS);
        return DEFAULT_UDFPS_BRIGHTNESS;
    }

    private static float parseFloatOr(String s, float fallback) {
        if (s == null) return fallback;
        try {
            return Float.parseFloat(s.trim());
        } catch (NumberFormatException e) {
            return fallback;
        }
    }
}
