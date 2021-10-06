/*
 * Copyright (C) 2015 The CyanogenMod Project
 *               2017-2019 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.lineageos.settings.doze;

import static android.provider.Settings.Secure.DOZE_ALWAYS_ON;
import static android.provider.Settings.Secure.DOZE_ENABLED;

import android.content.Context;
import android.content.Intent;
import android.hardware.display.AmbientDisplayConfiguration;
import android.os.UserHandle;
import android.provider.Settings;
import android.util.Log;

import androidx.preference.PreferenceManager;

public final class Utils {

    private static final String TAG = "DozeUtils";
    private static final boolean DEBUG = false;

    protected static final String ALWAYS_ON_DISPLAY = "always_on_display";
    protected static final String DOZE_ENABLE = "doze_enable";
    protected static final String GESTURE_HAND_WAVE_KEY = "gesture_hand_wave";
    protected static final String GESTURE_POCKET_KEY = "gesture_pocket";
    protected static final String WAKE_ON_GESTURE_KEY = "wake_on_gesture";

    protected static void startService(Context context) {
        if (DEBUG) Log.d(TAG, "Starting service");
        context.startServiceAsUser(new Intent(context, SamsungDozeService.class),
                UserHandle.CURRENT);
    }

    protected static void stopService(Context context) {
        if (DEBUG) Log.d(TAG, "Stopping service");
        context.stopServiceAsUser(new Intent(context, SamsungDozeService.class),
                UserHandle.CURRENT);
    }

    protected static void checkDozeService(Context context) {
        if (!isAlwaysOnEnabled(context) &&
                isDozeEnabled(context) && isAnyGestureEnabled(context)) {
            startService(context);
        } else {
            stopService(context);
        }
    }

    protected static boolean alwaysOnDisplayAvailable(Context context) {
        return new AmbientDisplayConfiguration(context).alwaysOnAvailable();
    }

    protected static boolean isAlwaysOnEnabled(Context context) {
        final boolean enabledByDefault = context.getResources()
                .getBoolean(com.android.internal.R.bool.config_dozeAlwaysOnEnabled);

        return Settings.Secure.getIntForUser(context.getContentResolver(),
                DOZE_ALWAYS_ON, alwaysOnDisplayAvailable(context) && enabledByDefault ? 1 : 0,
                UserHandle.USER_CURRENT) != 0;
    }

    protected static boolean isDozeEnabled(Context context) {
        return Settings.Secure.getInt(context.getContentResolver(),
                DOZE_ENABLED, 1) != 0;
    }

    protected static boolean enableAlwaysOn(Context context, boolean enable) {
        return Settings.Secure.putIntForUser(context.getContentResolver(),
                DOZE_ALWAYS_ON, enable ? 1 : 0, UserHandle.USER_CURRENT);
    }

    protected static boolean enableDoze(Context context, boolean enable) {
        return Settings.Secure.putInt(context.getContentResolver(),
                DOZE_ENABLED, enable ? 1 : 0);
    }

    protected static boolean isAnyGestureEnabled(Context context) {
        return isHandwaveGestureEnabled(context) || isPocketGestureEnabled(context);
    }

    protected static boolean isHandwaveGestureEnabled(Context context) {
        return PreferenceManager.getDefaultSharedPreferences(context)
                .getBoolean(GESTURE_HAND_WAVE_KEY, false);
    }

    protected static boolean isPocketGestureEnabled(Context context) {
        return PreferenceManager.getDefaultSharedPreferences(context)
                .getBoolean(GESTURE_POCKET_KEY, false);
    }

    protected static boolean isWakeOnGestureEnabled(Context context) {
        return PreferenceManager.getDefaultSharedPreferences(context)
                .getBoolean(WAKE_ON_GESTURE_KEY, false);
    }
}
