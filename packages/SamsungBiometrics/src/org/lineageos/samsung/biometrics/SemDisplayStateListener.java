/*
 * Copyright (C) 2025 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.samsung.biometrics;

import android.content.Context;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.util.Log;

import com.android.internal.statusbar.IStatusBarService;

import android.hardware.biometrics.IBiometricContextListener;

public class SemDisplayStateListener {
    private static final String TAG = "SemDisplayStateListener";

    private final Context mContext;
    private final IStatusBarService mStatusBarService;
    private final SemFodModeController mFodController;

    private final IBiometricContextListener mContextListener =
            new IBiometricContextListener.Stub() {
                @Override
                public void onFoldChanged(int foldState) {
                    Log.i(TAG, "Fold changed: " + foldState);
                }

                @Override
                public void onDisplayStateChanged(int displayState) {
                    Log.i(TAG, "Display state changed: " + displayState);
                    handleDisplayState(displayState);
                    mFodController.onDisplayStateChanged(displayState);
                }

                @Override
                public void onHardwareIgnoreTouchesChanged(boolean shouldIgnore) {
                    Log.i(TAG, "Hardware ignore touches changed: " + shouldIgnore);
                }
            };

    public SemDisplayStateListener(Context context,
                                   SemFodModeController fodController) {
        mContext = context;
        mFodController = fodController;
        mStatusBarService = IStatusBarService.Stub.asInterface(
                ServiceManager.getService("statusbar"));
    }

    public void register() {
        if (mStatusBarService == null) {
            Log.e(TAG, "StatusBarService not available");
            return;
        }
        try {
            Log.i(TAG, "Registering BiometricContextListener for display events");
            // There are no typos. Biometic is correct...
            mStatusBarService.setBiometicContextListener(mContextListener);
        } catch (RemoteException e) {
            Log.e(TAG, "Failed to register context listener", e);
        }
    }

    public void unregister() {
        if (mStatusBarService == null) return;
        try {
            mStatusBarService.setBiometicContextListener(null);
        } catch (RemoteException e) {
            Log.w(TAG, "Failed to unregister context listener", e);
        }
    }

    private void handleDisplayState(int displayState) {
        switch (displayState) {
            case 0:
                Log.i(TAG, "Display state: Unknown");
                break;
            case 1:
                Log.i(TAG, "Display state: Lockscreen");
                break;
            case 2:
                Log.i(TAG, "Display state: OFF");
                break;
            case 3:
                Log.i(TAG, "Display state: ScreenSaver");
                break;
            case 4:
                Log.i(TAG, "Display state: AoD");
                break;
            default:
                Log.i(TAG, "Other display state: " + displayState);
        }
    }
}
