/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.mdnieservice;

import android.content.ComponentName;
import android.content.Context;
import android.provider.Settings;
import android.util.Log;

public final class ServiceHelper {

    private static final String TAG = "ServiceHelper";
    private static final String SEPARATOR = ":";

    private ServiceHelper() {}

    public static void ensureServiceEnabled(Context context) {
        ComponentName cn = new ComponentName(context, MdnieService.class);
        String flat = cn.flattenToString();

        String current = Settings.Secure.getString(
                context.getContentResolver(),
                Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES);
        if (current == null) current = "";

        if (!current.contains(flat)) {
            String newValue = current.isEmpty() ? flat : current + SEPARATOR + flat;
            writeSecureSetting(context,
                    Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES, newValue);
            Log.d(TAG, "MdnieService registered in accessibility list");
        }

        // Always ensure the master accessibility switch is on.
        writeSecureIntSetting(context, Settings.Secure.ACCESSIBILITY_ENABLED, 1);
        Log.d(TAG, "Accessibility enabled");
    }

    public static boolean isServiceEnabled(Context context) {
        ComponentName cn = new ComponentName(context, MdnieService.class);
        String flat = cn.flattenToString();
        String current = Settings.Secure.getString(
                context.getContentResolver(),
                Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES);
        return current != null && current.contains(flat);
    }

    private static void writeSecureSetting(Context context, String key, String value) {
        try {
            Settings.Secure.putString(context.getContentResolver(), key, value);
        } catch (SecurityException e) {
            Log.e(TAG, "Missing WRITE_SECURE_SETTINGS – cannot write " + key, e);
        }
    }

    private static void writeSecureIntSetting(Context context, String key, int value) {
        try {
            Settings.Secure.putInt(context.getContentResolver(), key, value);
        } catch (SecurityException e) {
            Log.e(TAG, "Missing WRITE_SECURE_SETTINGS – cannot write " + key, e);
        }
    }
}
