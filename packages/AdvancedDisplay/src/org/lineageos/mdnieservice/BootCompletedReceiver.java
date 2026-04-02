/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.mdnieservice;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

import org.lineageos.internal.util.FileUtils;

public class BootCompletedReceiver extends BroadcastReceiver {

    private static final String TAG = "SamsungMdnie:Boot";

    @Override
    public void onReceive(Context context, Intent intent) {
        if (!Intent.ACTION_BOOT_COMPLETED.equals(intent.getAction())) return;

        Log.d(TAG, "Boot completed – initialising mDNIe service");

        String scenarioFile = context.getResources()
                .getString(R.string.mdnie_scenario_sysfs_file);
        if (FileUtils.isFileWritable(scenarioFile)) {
            FileUtils.writeLine(scenarioFile, Constants.DEFAULT_MDNIE_SCENARIO);
            Log.d(TAG, "Default mDNIe scenario written: " + Constants.DEFAULT_MDNIE_SCENARIO);
        } else {
            Log.w(TAG, "mDNIe scenario sysfs node not writable: " + scenarioFile);
        }

        ServiceHelper.ensureServiceEnabled(context);
    }
}
