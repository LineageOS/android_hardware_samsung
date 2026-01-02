/*
 * Copyright (C) 2025 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.samsung.biometrics;

import android.content.Context;
import android.util.Slog;

import java.io.File;
import java.nio.charset.StandardCharsets;

public class SamsungOpticalUdfpsService implements DisplayStateListener.OnDisplayStateChangedListener,
BioStateListener.OnBiometricStateChangedListener {
    private static final String TAG = "SB_OpticalUdfpsService";
    private static final String MAX_BRIGHTNESS_PATH = "/sys/class/lcd/panel/mask_brightness";

    private final DisplayStateListener mDisplayStateListener;
    private final BioStateListener mBioStateListener;
    private final MaskView mMaskView;

    private float mUdfpsBrightness;
    private float mUdfpsNits;

    private int mDisplayState;
    private int mBiometricState;

    public SamsungOpticalUdfpsService(Context context) {
        Slog.i(TAG, "SamsungOpticalUdfpsService is starting");

        mDisplayStateListener = new DisplayStateListener(context, this);
        mBioStateListener = new BioStateListener(context, this);

        mMaskView = new MaskView(context);

        mDisplayStateListener.register();
        mBioStateListener.register();

        // Initialize HBM Brightness
        mUdfpsBrightness = Float.parseFloat(context.getResources().getString(org.lineageos.samsung.biometrics.R.string.config_Display_Solution_Udfps_Brightness));
        mUdfpsNits = Float.parseFloat(context.getResources().getString(org.lineageos.samsung.biometrics.R.string.config_Display_Solution_Udfps_Nits));

        if (mUdfpsBrightness <= 0.0f) {
            mUdfpsBrightness = 319.0f;
            Slog.w(TAG, "Use default value for UDFPS brightness: " + mUdfpsBrightness);
        }

        if (mUdfpsNits <= 0.0f) {
            mUdfpsNits = 525.0f;
            Slog.w(TAG, "Set default value for UDFPS nits: " + mUdfpsNits);
        }

        mMaskView.setUdfpsParams(mUdfpsBrightness, mUdfpsNits);

        byte[] bytes = Integer.toString((int) Math.round(mUdfpsBrightness)).getBytes(StandardCharsets.UTF_8);
        Utils.writeFile(new File(MAX_BRIGHTNESS_PATH), bytes);
    }

    public void destroy() {
        Slog.i(TAG, "Destroying SamsungOpticalUdfpsService");
        mDisplayStateListener.unregister();
        mBioStateListener.unregister();
        mMaskView.hide();
        mMaskView.stop();
    }

    @Override
    public void onDisplayStateChanged(int displayState) {
        mDisplayState = displayState;
        updateMaskView();
    }

    @Override
    public void onBiometricStateChanged(int newState) {
        mBiometricState = newState;
        updateMaskView();
    }

    private void updateMaskView() {
        boolean isAod = mDisplayState == 4; // AoD

        switch (mBiometricState) {
            case 0: // IDLE
                mMaskView.hide();
                mMaskView.stop();
                break;
            case 1: // ENROLLING
                mMaskView.show();
                mMaskView.start();
                break;
            case 2: // KEYGUARD_AUTH
                if (isAod) {
                    mMaskView.hide();
                    mMaskView.stop();
                } else {
                    mMaskView.show();
                    mMaskView.start();
                }
                break;
            case 3: // BIOMETRIC_PROMPT_AUTH
                mMaskView.show();
                mMaskView.start();
                break;
            default:
                break;
        }
    }
}
