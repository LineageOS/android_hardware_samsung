/*
 * SPDX-FileCopyrightText: 2021-2022 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.spenactions;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.UserHandle;
import android.util.Log;

public class BootReceiver extends BroadcastReceiver {

    private static final String LOG_TAG = "SPenActions/BootReceiver";

    @Override
    public void onReceive(Context context, Intent intent) {
        if (Intent.ACTION_LOCKED_BOOT_COMPLETED.equalsIgnoreCase(intent.getAction())) {
            Log.i(LOG_TAG, "Starting service");

            context.startServiceAsUser(new Intent(context, SPenActionsService.class),
                    UserHandle.CURRENT);
        }
    }
}
