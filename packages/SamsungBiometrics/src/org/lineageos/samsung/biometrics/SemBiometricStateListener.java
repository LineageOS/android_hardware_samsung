/*
 * Copyright (C) 2025 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.samsung.biometrics;

import android.annotation.NonNull;
import android.content.Context;
import android.hardware.biometrics.BiometricStateListener;
import android.hardware.fingerprint.FingerprintManager;
import android.hardware.fingerprint.FingerprintSensorPropertiesInternal;
import android.hardware.fingerprint.IFingerprintAuthenticatorsRegisteredCallback;
import android.util.Log;

import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

public class SemBiometricStateListener {
    private static final String TAG = "SemBiometricStateListener";

    @NonNull
    private final Context mContext;
    private final SemFodModeController mFodController;

    private FingerprintManager mFingerprintManager;
    private BiometricStateListener mListener;
    private final AtomicBoolean mRegistered = new AtomicBoolean(false);

    public SemBiometricStateListener(@NonNull Context context, SemFodModeController fodController) {
        mContext = context;
        mFodController = fodController;
    }

    public void register() {
        registerSemBiometricStateListener();
    }

    public void unregister() {
    }

    private void registerSemBiometricStateListener() {
        mFingerprintManager = mContext.getSystemService(FingerprintManager.class);
        if (mFingerprintManager == null) {
            Log.w(TAG, "FingerprintManager is null");
            return;
        }

        if (mListener == null) {
            mListener = new BiometricStateListener() {
                @Override
                public void onStateChanged(@BiometricStateListener.State int newState) {
                    handleBiometricState(newState);
                    mFodController.onBiometricStateChanged(newState);
                }

                @Override
                public void onBiometricAction(@BiometricStateListener.Action int action) {
                    Log.d(TAG, "onBiometricAction " + action);
                    // Do something???
                }
            };
        }

        mFingerprintManager.addAuthenticatorsRegisteredCallback(
                new IFingerprintAuthenticatorsRegisteredCallback.Stub() {
                    @Override
                    public void onAllAuthenticatorsRegistered(
                            List<FingerprintSensorPropertiesInternal> sensors) {

                        if (!mRegistered.compareAndSet(false, true)) {
                            Log.d(TAG, "Already registered, skipping");
                            return;
                        }

                        try {
                            mFingerprintManager.registerBiometricStateListener(mListener);
                            Log.i(TAG, "BiometricStateListener registered");
                        } catch (Throwable t) {
                            mRegistered.set(false);
                            Log.e(TAG, "Failed to register BiometricStateListener", t);
                        }
                    }
                }
        );
    }

    private void handleBiometricState(int newState) {
        switch (newState) {
            case 0:
                Log.i(TAG, "Biometric state: Idle");
                break;
            case 1:
                Log.i(TAG, "Biometric state: Enrolling");
                break;
            case 2:
                Log.i(TAG, "Biometric state: Keyguard Auth");
                break;
            case 3:
                Log.i(TAG, "Biometric state: Biometric Prompt Auth");
                break;
            case 4:
                Log.i(TAG, "Biometric state: Other Auth");
                break;
            default:
                Log.i(TAG, "Unknown State??? " + newState);
        }
    }
}
