/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
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

public class BioStateListener {
    private static final String TAG = "SB_BioStateListener";

    public interface OnBiometricStateChangedListener {
        void onBiometricStateChanged(int newState);
    }

    @NonNull
    private final Context mContext;
    private final OnBiometricStateChangedListener mListenerCallback;

    private FingerprintManager mFingerprintManager;
    private BiometricStateListener mListener;
    private final AtomicBoolean mRegistered = new AtomicBoolean(false);

    public BioStateListener(@NonNull Context context,
            OnBiometricStateChangedListener listenerCallback) {
        mContext = context;
        mListenerCallback = listenerCallback;
    }

    public void register() {
        registerBioStateListener();
    }

    public void unregister() {
    }

    private void registerBioStateListener() {
        mFingerprintManager = mContext.getSystemService(FingerprintManager.class);
        if (mFingerprintManager == null) {
            Log.w(TAG, "FingerprintManager is null");
            return;
        }

        if (mListener == null) {
            mListener = new BiometricStateListener() {
                @Override
                public void onStateChanged(@BiometricStateListener.State int newState) {
                    mListenerCallback.onBiometricStateChanged(newState);
                }

                @Override
                public void onBiometricAction(@BiometricStateListener.Action int action) {
                    Log.d(TAG, "onBiometricAction " + action);
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
                            Log.i(TAG, "BioStateListener registered");
                        } catch (Throwable t) {
                            mRegistered.set(false);
                            Log.e(TAG, "Failed to register BioStateListener", t);
                        }
                    }
                });
    }
}
