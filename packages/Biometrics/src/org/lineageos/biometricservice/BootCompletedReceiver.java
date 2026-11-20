/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.biometricservice;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

/**
 * Starts {@link SamsungBiometricService} on direct-boot and normal boot completion.
 */
public class BootCompletedReceiver extends BroadcastReceiver {
    private static final String TAG = "SB_BootReceiver";

    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent == null)
            return;
        String action = intent.getAction();
        if (Intent.ACTION_LOCKED_BOOT_COMPLETED.equals(action)
                || Intent.ACTION_BOOT_COMPLETED.equals(action)) {
            Log.i(TAG, "Received " + action + ": starting SamsungBiometricService");
            Intent serviceIntent = new Intent(context, SamsungBiometricService.class);
            context.startService(serviceIntent);
        }
    }
}
