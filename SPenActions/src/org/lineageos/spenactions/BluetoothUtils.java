/*
 * SPDX-FileCopyrightText: 2022 The LineageOS Project
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

import java.util.List;
import java.util.UUID;

import vendor.samsung.hardware.spen.ISPen;

public class BluetoothUtils {

    private static final String LOG_TAG = "SPenActions/BluetoothUtils";

    public static void resetSPenMAC(Context context) {
        ISPen hal = ISPen.Stub.asInterface(ServiceManager.waitForDeclaredService(
                ISPen.DESCRIPTOR + "/default"));
        BluetoothManager manager = (BluetoothManager) context.getSystemService(Context.BLUETOOTH_SERVICE);
        BluetoothAdapter adapter = manager.getAdapter();

        if (!adapter.isEnabled())
            return;

        try {
            // Toggle charging to make pen discoverable for a few seconds
            hal.setCharging(false);
            hal.setCharging(true);
        } catch (RemoteException ex) {
            ex.printStackTrace();
        }

        BluetoothLeScanner scanner = adapter.getBluetoothLeScanner();
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
                // Simulate Bluetooth switch on/off to apply the new MAC
                Intent bluetoothState = new Intent(BluetoothAdapter.ACTION_STATE_CHANGED);
                bluetoothState.setPackage(context.getPackageName());
                bluetoothState.putExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.STATE_OFF);
                context.sendBroadcastAsUser(bluetoothState, UserHandle.CURRENT);
                bluetoothState.putExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.STATE_ON);
                context.sendBroadcastAsUser(bluetoothState, UserHandle.CURRENT);
            }
        };
        ScanFilter filter = new ScanFilter.Builder()
                .setServiceUuid(new ParcelUuid(UUID.fromString(
                        context.getResources().getString(R.string.config_sPenServiceUuid))))
                .build();
        ScanSettings settings = new ScanSettings.Builder()
                .setCallbackType(ScanSettings.CALLBACK_TYPE_FIRST_MATCH)
                .build();

        scanner.startScan(List.of(filter), settings, callback);
        new Handler(Looper.getMainLooper()).postDelayed(() -> {
            scanner.stopScan(callback);
        }, 60 * 1000); // Cancel scan after 60s
    }

}
