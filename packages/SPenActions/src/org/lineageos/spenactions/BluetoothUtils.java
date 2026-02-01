/*
 * SPDX-FileCopyrightText: 2022-2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.spenactions;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothManager;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelUuid;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.os.UserHandle;
import android.util.Log;

import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

import vendor.samsung.hardware.spen.ISPen;

public class BluetoothUtils {

    private static final String LOG_TAG = "SPenActions/BluetoothUtils";

    public static void resetSPenMAC(Context context) {
        ISPen hal = ISPen.Stub.asInterface(ServiceManager.waitForDeclaredService(
                ISPen.DESCRIPTOR + "/default"));

        BluetoothAdapter btAdapter = context.getSystemService(BluetoothManager.class).getAdapter();
        if (!btAdapter.isEnabled())
            return;

        try {
            // Toggle charging to make pen discoverable for a few seconds
            hal.setCharging(false);
            hal.setCharging(true);
        } catch (RemoteException ex) {
            ex.printStackTrace();
        }

        BluetoothLeScanner scanner = btAdapter.getBluetoothLeScanner();
        ScanCallback callback = new ScanCallback() {
            @Override
            public void onScanResult(int callbackType, ScanResult result) {
                Log.i(LOG_TAG, result.getDevice().getName() + " RSSI: " + result.getRssi());
                try {
                    hal.setMACAddress(result.getDevice().getAddress());
                } catch (RemoteException ex) {
                    ex.printStackTrace();
                }
                scanner.stopScan(this);
                // Simulate Bluetooth switch off/on to apply the new MAC
                simulateReconnection(context);
            }
        };
        List<ScanFilter> filters = new ArrayList<>();
        for (UUID uuid : SPenIdentity.getServiceUUIDs()) {
            filters.add(new ScanFilter.Builder()
                    .setServiceUuid(new ParcelUuid(uuid))
                    .build());
        }
        ScanSettings settings = new ScanSettings.Builder()
                .setCallbackType(ScanSettings.CALLBACK_TYPE_FIRST_MATCH)
                .build();

        scanner.startScan(filters, settings, callback);
        new Handler(Looper.getMainLooper()).postDelayed(() -> {
            scanner.stopScan(callback);
        }, 60 * 1000); // Cancel scan after 60s
    }

    public static void simulateReconnection(Context context) {
        Intent btState = new Intent(BluetoothAdapter.ACTION_STATE_CHANGED);
        btState.setPackage(context.getPackageName());
        btState.putExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.STATE_OFF);
        context.sendBroadcastAsUser(btState, UserHandle.CURRENT);
        btState.putExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.STATE_ON);
        context.sendBroadcastAsUser(btState, UserHandle.CURRENT);
    }
}
