/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.mdnieservice;

import android.content.Context;
import android.content.res.Resources;
import android.util.Log;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

public final class AppScenarioConfig {

    private static final String TAG = "AppScenarioConfig";

    /**
     * Scenario value string for each named array resource, in declaration order.
     * Must stay in sync with the arrays listed in config.xml.
     */
    private static final String[][] RESOURCE_SCENARIO_PAIRS = {
        { "mdnie_video_apps",       "1" },
        { "mdnie_camera_apps",      "4" },
        { "mdnie_navigation_apps",  "5" },
        { "mdnie_gallery_apps",     "6" },
        { "mdnie_vcall_apps",       "7" },
    };

    private static volatile Map<String, String> sMap = null;

    private AppScenarioConfig() {}

    public static String getScenarioForPackage(Context context, String packageName) {
        return getMap(context).get(packageName);
    }

    private static Map<String, String> getMap(Context context) {
        if (sMap == null) {
            synchronized (AppScenarioConfig.class) {
                if (sMap == null) {
                    sMap = buildMap(context.getApplicationContext());
                }
            }
        }
        return sMap;
    }

    private static Map<String, String> buildMap(Context context) {
        final Resources res = context.getResources();
        final String pkg = context.getPackageName();
        final Map<String, String> map = new HashMap<>();

        for (final String[] pair : RESOURCE_SCENARIO_PAIRS) {
            final String arrayName    = pair[0];
            final String scenarioVal  = pair[1];

            final int resId = res.getIdentifier(arrayName, "array", pkg);
            if (resId == 0) {
                Log.w(TAG, "Resource array not found: " + arrayName
                        + " – skipping scenario " + scenarioVal);
                continue;
            }

            final String[] packages;
            try {
                packages = res.getStringArray(resId);
            } catch (Resources.NotFoundException e) {
                Log.w(TAG, "Could not load array " + arrayName, e);
                continue;
            }

            for (final String packageName : packages) {
                if (packageName == null || packageName.isEmpty()) continue;

                final String existing = map.put(packageName, scenarioVal);
                if (existing != null && !existing.equals(scenarioVal)) {
                    // A package appears in two arrays; the later array wins.
                    // This should be treated as a configuration error.
                    Log.w(TAG, "Package " + packageName
                            + " mapped to scenario " + existing
                            + " and overridden to " + scenarioVal
                            + " – check overlay arrays for duplicates");
                }
            }

            Log.d(TAG, "Loaded " + packages.length + " packages for scenario "
                    + scenarioVal + " (" + arrayName + ")");
        }

        Log.i(TAG, "Built scenario map: " + map.size() + " total package entries");
        return Collections.unmodifiableMap(map);
    }
}
