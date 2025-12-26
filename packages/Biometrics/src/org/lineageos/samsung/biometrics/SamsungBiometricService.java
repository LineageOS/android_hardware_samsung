/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.samsung.biometrics;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.util.Log;

public class SamsungBiometricService extends Service {
    private static final String TAG = "SB_Service";

    @Override
    public void onCreate() {
        super.onCreate();
        Log.i(TAG, "SamsungBiometricService is alive");
    }

    @Override
    public void onDestroy() {
        Log.i(TAG, "SamsungBiometricService is killed");
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        // This service does not support binding
        return null;
    }
}
