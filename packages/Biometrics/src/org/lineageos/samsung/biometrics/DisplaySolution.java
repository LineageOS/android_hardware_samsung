/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.samsung.biometrics;

import android.content.Context;
import android.util.Log;

import org.lineageos.samsung.biometrics.R;

public class DisplaySolution {

    private static final String TAG = "SB_DisplaySolution";

    private final String[] mBrightnessBacklightValueStringArray;
    private final String[] mBrightnessNitsValueStringArray;
    private final String[] mGammaStringArray;

    private final float[] mGammaArray;

    public DisplaySolution(Context context) {
        mBrightnessBacklightValueStringArray = context.getResources().getStringArray(
                R.array.config_Screen_Brightness_Backlight_Value);
        mBrightnessNitsValueStringArray = context.getResources().getStringArray(
                R.array.config_Screen_Brightness_Nits_Value);
        mGammaStringArray = context.getResources().getStringArray(
                R.array.config_Display_Solution_Gamma_Value);

        if (mGammaStringArray != null) {
            mGammaArray = new float[mGammaStringArray.length];
            for (int i = 0; i < mGammaStringArray.length; i++) {
                try {
                    mGammaArray[i] = Float.parseFloat(mGammaStringArray[i]);
                } catch (NumberFormatException e) {
                    Log.e(TAG, "Failed to parse gamma value: " + mGammaStringArray[i]);
                    mGammaArray[i] = 2.2f; // Default fallback
                }
            }
        } else {
            mGammaArray = new float[]{2.2f}; // Safe default
        }
    }

    public final float getAlphaMaskLevel(float currentBrightnessIndex, float fingerPrintIndex,
            float brCtrl) {
        float gamma = (mGammaArray.length > 0) ? mGammaArray[0] : 2.2f;

        Log.d(TAG, "getAlphaMaskLevel() CurrentPlatformBrightnessValue : " + currentBrightnessIndex
                + " , FingerPrintPlatformValue : " + fingerPrintIndex
                + " , br_ctrl : " + brCtrl
                + " , gamma : " + gamma);

        float currentNits = currentBrightnessIndex >= 0.0f
                ? Float.parseFloat(mBrightnessNitsValueStringArray[(int) currentBrightnessIndex])
                : -1.0f;

        float targetNits = fingerPrintIndex >= 0.0f
                ? Float.parseFloat(mBrightnessNitsValueStringArray[(int) fingerPrintIndex])
                : -1.0f;

        Log.i(TAG, "calculateAlphaBlendingValue() currentNits : " + currentNits
                + " , targetNits : " + targetNits
                + " , br_ctrl : " + brCtrl
                + " , gamma : " + gamma);

        if (targetNits == 0) return 0.0f;

        double base = (currentNits * brCtrl) / targetNits;
        double exponent = 1.0f / gamma;

        return (float) (1.0d - Math.pow(base, exponent));
    }

    public final float getFingerPrintBacklightValue(int requiredNits) {
        for (int i = 0; i < mBrightnessNitsValueStringArray.length; i++) {
            int thresholdNits = Integer.parseInt(mBrightnessNitsValueStringArray[i]);

            if (requiredNits <= thresholdNits) {
                float backlightValue = Float.parseFloat(mBrightnessBacklightValueStringArray[i]);

                Log.d(TAG, "getFingerPrintBacklightValue() brightnessNits : " + requiredNits
                        + " , FingerPrintBacklightValue : " + backlightValue
                        + " , Size : " + mBrightnessNitsValueStringArray.length);

                return backlightValue;
            }
        }

        return -1.0f;
    }
}
