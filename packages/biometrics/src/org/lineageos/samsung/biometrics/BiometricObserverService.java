/*
 * Copyright (C) 2025 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.samsung.biometrics;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.util.Log;

public class BiometricObserverService extends Service {
    private static final String TAG = "BiometricObserverService";

    private SemDisplayStateListener mDisplayObserver;
    private SemBiometricStateListener mBiometricObserver;
    private SemFodModeController mFodController;
    private SemFodRectCalculator mFodRectCalculator;

    @Override
    public void onCreate() {
        super.onCreate();
        Log.i(TAG, "BiometricObserverService created");

        mFodController = new SemFodModeController();
        mFodRectCalculator = new SemFodRectCalculator(this);
        mDisplayObserver = new SemDisplayStateListener(this, mFodController);
        mBiometricObserver = new SemBiometricStateListener(this, mFodController);

        mFodRectCalculator.writeFodRect();
        mDisplayObserver.register();
        mBiometricObserver.register();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.i(TAG, "BiometricObserverService started");
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        Log.i(TAG, "BiometricObserverService destroyed");

        if (mDisplayObserver != null) mDisplayObserver.unregister();
        if (mBiometricObserver != null) mBiometricObserver.unregister();

        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
