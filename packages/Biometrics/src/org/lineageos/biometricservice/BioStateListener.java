/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.biometricservice;

import android.annotation.NonNull;
import android.content.Context;
import android.hardware.biometrics.AuthenticationStateListener;
import android.hardware.biometrics.BiometricManager;
import android.hardware.biometrics.BiometricSourceType;
import android.hardware.biometrics.BiometricStateListener;
import android.hardware.biometrics.events.AuthenticationAcquiredInfo;
import android.hardware.biometrics.events.AuthenticationErrorInfo;
import android.hardware.biometrics.events.AuthenticationFailedInfo;
import android.hardware.biometrics.events.AuthenticationHelpInfo;
import android.hardware.biometrics.events.AuthenticationStartedInfo;
import android.hardware.biometrics.events.AuthenticationStoppedInfo;
import android.hardware.biometrics.events.AuthenticationSucceededInfo;
import android.hardware.fingerprint.FingerprintManager;
import android.hardware.fingerprint.FingerprintSensorPropertiesInternal;
import android.hardware.fingerprint.IFingerprintAuthenticatorsRegisteredCallback;
import android.util.Log;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Registers with the framework for biometric state changes and authentication events,
 * and reports the first fingerprint sensor's properties once authenticators are up.
 *
 * <p>Callbacks arrive on binder threads.
 */
public class BioStateListener {
    private static final String TAG = "SB_BioStateListener";

    public interface OnBiometricStateChangedListener {
        void onBiometricStateChanged(int newState);
        default void onAuthenticationAcquired(int acquiredInfo) {}
        default void onAuthenticationFailed() {}
        default void onAuthenticationError(int errorCode, String errString) {}
        default void onAuthenticationSucceeded() {}
    }

    public interface OnSensorPropertiesListener {
        /** Called once, before state callbacks start, with the first fingerprint sensor. */
        void onSensorPropertiesAvailable(FingerprintSensorPropertiesInternal props);
    }

    @NonNull private final Context mContext;
    private final OnBiometricStateChangedListener mListenerCallback;
    private final OnSensorPropertiesListener mSensorCallback;

    private FingerprintManager mFingerprintManager;
    private BiometricManager mBiometricManager;
    private BiometricStateListener mListener;
    private final AtomicBoolean mRegistered = new AtomicBoolean(false);

    private final AuthenticationStateListener mAuthStateListener =
            new AuthenticationStateListener.Stub() {
                @Override
                public void onAuthenticationAcquired(AuthenticationAcquiredInfo authInfo) {
                    if (authInfo != null
                            && authInfo.getBiometricSourceType()
                                    == BiometricSourceType.FINGERPRINT) {
                        mListenerCallback.onAuthenticationAcquired(authInfo.getAcquiredInfo());
                    }
                }

                @Override
                public void onAuthenticationFailed(AuthenticationFailedInfo authInfo) {
                    if (authInfo != null
                            && authInfo.getBiometricSourceType()
                                    == BiometricSourceType.FINGERPRINT) {
                        mListenerCallback.onAuthenticationFailed();
                    }
                }

                @Override
                public void onAuthenticationError(AuthenticationErrorInfo authInfo) {
                    if (authInfo != null
                            && authInfo.getBiometricSourceType()
                                    == BiometricSourceType.FINGERPRINT) {
                        mListenerCallback.onAuthenticationError(
                                authInfo.getErrCode(), authInfo.getErrString());
                    }
                }

                @Override
                public void onAuthenticationHelp(AuthenticationHelpInfo authInfo) {}

                @Override
                public void onAuthenticationStarted(AuthenticationStartedInfo authInfo) {}

                @Override
                public void onAuthenticationStopped(AuthenticationStoppedInfo authInfo) {}

                @Override
                public void onAuthenticationSucceeded(AuthenticationSucceededInfo authInfo) {
                    if (authInfo != null
                            && authInfo.getBiometricSourceType()
                                    == BiometricSourceType.FINGERPRINT) {
                        mListenerCallback.onAuthenticationSucceeded();
                    }
                }
            };

    public BioStateListener(
            @NonNull Context context, OnBiometricStateChangedListener listenerCallback) {
        this(context, listenerCallback, null);
    }

    public BioStateListener(@NonNull Context context,
            OnBiometricStateChangedListener listenerCallback,
            OnSensorPropertiesListener sensorCallback) {
        mContext = context;
        mListenerCallback = listenerCallback;
        mSensorCallback = sensorCallback;
    }

    // ---- Lifecycle ----

    public void register() {
        registerBioStateListener();
        registerAuthenticationStateListener();
    }

    public void unregister() {
        if (!mRegistered.get())
            return;
        mRegistered.set(false);
        if (mBiometricManager != null) {
            try {
                mBiometricManager.unregisterAuthenticationStateListener(mAuthStateListener);
                Log.i(TAG, "AuthenticationStateListener unregistered");
            } catch (Exception e) {
                Log.w(TAG, "Failed to unregister AuthenticationStateListener", e);
            }
        }
    }

    // ---- Registration ----

    private void registerAuthenticationStateListener() {
        mBiometricManager = mContext.getSystemService(BiometricManager.class);
        if (mBiometricManager == null) {
            Log.w(TAG, "BiometricManager is null");
            return;
        }
        try {
            mBiometricManager.registerAuthenticationStateListener(mAuthStateListener);
            Log.i(TAG, "AuthenticationStateListener registered");
        } catch (Exception e) {
            Log.e(TAG, "Failed to register AuthenticationStateListener", e);
        }
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

        List<FingerprintSensorPropertiesInternal> existing =
                mFingerprintManager.getSensorPropertiesInternal();
        if (existing != null && !existing.isEmpty()) {
            if (mRegistered.compareAndSet(false, true)) {
                if (mSensorCallback != null) {
                    try {
                        mSensorCallback.onSensorPropertiesAvailable(existing.get(0));
                    } catch (Throwable t) {
                        Log.e(TAG, "Sensor properties callback failed", t);
                    }
                }
                try {
                    mFingerprintManager.registerBiometricStateListener(mListener);
                    Log.i(TAG, "BioStateListener registered immediately from existing properties");
                } catch (Throwable t) {
                    mRegistered.set(false);
                    Log.e(TAG, "Failed to register BioStateListener", t);
                }
            }
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

                        if (mSensorCallback != null && sensors != null && !sensors.isEmpty()) {
                            try {
                                mSensorCallback.onSensorPropertiesAvailable(sensors.get(0));
                            } catch (Throwable t) {
                                Log.e(TAG, "Sensor properties callback failed", t);
                            }
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
