/*
 * SPDX-FileCopyrightText: 2021-2022 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.spenactions.settings;

import android.content.Context;
import android.content.SharedPreferences.Editor;
import android.provider.Settings;

import androidx.preference.PreferenceManager;

public class SettingsUtils {

    public static final String SPEN_BLUETOOTH_ENABLE = "spen_bluetooth_enable";
    public static final String SPEN_MODE = "spen_mode";
    public static final String ACTION_BUTTONS = "action_buttons";

    public static boolean isEnabled(Context context, String pref, boolean defValue) {
        return PreferenceManager.getDefaultSharedPreferences(context)
                .getBoolean(pref, defValue);
    }

    public static String getSwitchPreference(Context context, String pref, String defValue) {
        return PreferenceManager.getDefaultSharedPreferences(context)
                .getString(pref, defValue);
    }
}
