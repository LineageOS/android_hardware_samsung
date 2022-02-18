/*
 * SPDX-FileCopyrightText: 2021-2022 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.spenactions;

import android.app.Service;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothManager;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.IBinder;
import android.os.UserHandle;
import android.util.Log;

public class SPenActionsService extends Service {

    private static final String LOG_TAG = "SPenActions/Service";

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.d(LOG_TAG, "Service started");

        BluetoothAdapter btAdapter = getSystemService(BluetoothManager.class).getAdapter();

        IntentFilter btFilter = new IntentFilter();
        btFilter.addAction(BluetoothAdapter.ACTION_STATE_CHANGED);
        registerReceiver(new BluetoothReceiver(this), btFilter);

        // Just started - manually send current bluetooth state
        Intent btState = new Intent(BluetoothAdapter.ACTION_STATE_CHANGED);
        btState.setPackage(getPackageName());
        btState.putExtra(BluetoothAdapter.EXTRA_STATE, btAdapter.getState());
        sendBroadcastAsUser(btState, UserHandle.CURRENT);

        return super.onStartCommand(intent, flags, startId);
    }

    @Override
    public void onDestroy() {
        Log.d(LOG_TAG, "Service stopping");

        // Force stop by faking bluetooth turn off
        Intent btState = new Intent(BluetoothAdapter.ACTION_STATE_CHANGED);
        btState.setPackage(getPackageName());
        btState.putExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.STATE_OFF);
        sendBroadcastAsUser(btState, UserHandle.CURRENT);
    }
}
