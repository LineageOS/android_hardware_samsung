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
     * For each named array resource, the name of the config.xml integer resource
     * that holds the scenario value to apply to packages in that array.
     * Must stay in sync with the arrays and integers listed in config.xml.
     */
    private static final String[][] RESOURCE_SCENARIO_PAIRS = {
        { "mdnie_video_apps",       "config_scenario_video_mode" },
        { "mdnie_camera_apps",      "config_scenario_camera_mode" },
        { "mdnie_navigation_apps",  "config_scenario_navigation_mode" },
        { "mdnie_gallery_apps",     "config_scenario_gallery_mode" },
        { "mdnie_vcall_apps",       "config_scenario_videocall_mode" },
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
            final String arrayName     = pair[0];
            final String integerName   = pair[1];

            final int scenarioResId = res.getIdentifier(integerName, "integer", pkg);
            if (scenarioResId == 0) {
                Log.w(TAG, "Scenario integer resource not found: " + integerName
                        + " – skipping array " + arrayName);
                continue;
            }

            final String scenarioVal;
            try {
                scenarioVal = String.valueOf(res.getInteger(scenarioResId));
            } catch (Resources.NotFoundException e) {
                Log.w(TAG, "Could not load integer " + integerName, e);
                continue;
            }

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
