/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package com.samsung.android.soundbooster.stage;

import android.app.Application;
import android.content.Intent;
import android.util.Log;

public class SoundBoosterApp extends Application {
    private static final String TAG = "SoundBoosterStage";

    @Override
    public void onCreate() {
        super.onCreate();
        Log.i(TAG, "starting Stage service");

        Intent svc = new Intent(this, SoundBoosterStageService.class);
        try {
            startService(svc);
        } catch (Exception e) {
            Log.e(TAG, "Failed to start Stage service from Application", e);
            // Fallback to broadcast to BootReceiver
            try {
                sendBroadcast(new Intent("com.samsung.android.soundbooster.stage.RESTART"));
            } catch (Exception ignored) {
            }
        }
    }
}
