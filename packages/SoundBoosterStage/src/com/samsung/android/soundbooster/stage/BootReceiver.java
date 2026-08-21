/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package com.samsung.android.soundbooster.stage;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

public class BootReceiver extends BroadcastReceiver {
    private static final String TAG = "SoundBoosterStage";

    @Override
    public void onReceive(Context context, Intent intent) {
        String action = intent != null ? intent.getAction() : "";
        Log.i(TAG, "BootReceiver onReceive: " + action);

        Intent svc = new Intent(context, SoundBoosterStageService.class);
        try {
            context.startService(svc);
        } catch (Exception e) {
            Log.e(TAG, "start service failed", e);
            // Fallback: try restart broadcast
            try {
                context.sendBroadcast(new Intent("com.samsung.android.soundbooster.stage.RESTART"));
            } catch (Exception ignored) {
            }
        }
    }
}
