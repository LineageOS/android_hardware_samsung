/*
 * SPDX-FileCopyrightText: 2021-2022 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.spenactions;

import android.bluetooth.BluetoothAdapter;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.RemoteException;
import android.util.Log;

import org.lineageos.spenactions.settings.SettingsUtils;

public class BluetoothReceiver extends BroadcastReceiver {

    private final Context mContext;
    private final SPenConnectionManager mConnectionManager;

    public BluetoothReceiver(Context context) {
        this.mContext = context;
        this.mConnectionManager = new SPenConnectionManager(context);
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        try {
            int state = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, -1);

            if (state == BluetoothAdapter.STATE_ON) {
                if (SettingsUtils.isEnabled(mContext, SettingsUtils.SPEN_BLUETOOTH_ENABLE, false)) {
                    mConnectionManager.disconnect();
                    mConnectionManager.connect();
                }
            } else if (state == BluetoothAdapter.STATE_OFF) {
                mConnectionManager.disconnect();
            }
        } catch (RemoteException ex) {
            ex.printStackTrace();
        }
    }

}
