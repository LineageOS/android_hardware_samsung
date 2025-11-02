/*
 * Copyright (C) 2025 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package android.os;

public final class SemSystemProperties {

    private SemSystemProperties() {
    }

    public static String get(String key) {
        return SystemProperties.get(key);
    }

    public static String get(String key, String def) {
        return SystemProperties.get(key, def);
    }

    public static int getInt(String key, int def) {
        return SystemProperties.getInt(key, def);
    }

    public static long getLong(String key, long def) {
        return SystemProperties.getLong(key, def);
    }

    public static boolean getBoolean(String key, boolean def) {
        return SystemProperties.getBoolean(key, def);
    }

    public static void set(String key, String val) {
        SystemProperties.set(key, val);
    }

    public static String getSalesCode() {
        return SystemProperties.get("ro.csc.sales_code", "");
    }

    public static String getCountryCode() {
        return SystemProperties.get("ro.csc.country_code", "");
    }

    public static String getCountryIso() {
        return SystemProperties.get("ro.csc.countryiso_code", "");
    }
}
