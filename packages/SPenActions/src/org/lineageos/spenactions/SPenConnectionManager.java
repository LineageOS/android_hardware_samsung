/*
 * SPDX-FileCopyrightText: 2021-2022 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.spenactions;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.util.Log;

import vendor.samsung.hardware.spen.ISPen;

public class SPenConnectionManager extends BroadcastReceiver {

    private static final String LOG_TAG = "SPenActions/SPenConnectionManager";

    private final Context mContext;
    private ISPen mSPenHAL;
    private BluetoothDevice mSpen;
    private BluetoothGatt mGatt;
    private String mCurrentBleSpenAddr;

    public SPenConnectionManager(Context context) {
        mContext = context;
        mSPenHAL = ISPen.Stub.asInterface(ServiceManager.waitForDeclaredService(
                ISPen.DESCRIPTOR + "/default"));

        context.registerReceiver(this, new IntentFilter(BluetoothDevice.ACTION_PAIRING_REQUEST));
    }

    public void connect() throws RemoteException {
        mCurrentBleSpenAddr = mSPenHAL.getMACAddress();
        Log.i(LOG_TAG, "Connecting! SPen BLE address: " + mCurrentBleSpenAddr);

        BluetoothManager btManager = mContext.getSystemService(BluetoothManager.class);
        BluetoothAdapter btAdapter = btManager.getAdapter();
        if (!btAdapter.isEnabled()) {
            btAdapter.enable();
        }

        mSpen = btAdapter.getRemoteDevice(mCurrentBleSpenAddr);

        if (!mSPenHAL.isCharging()) {
            mSPenHAL.setCharging(true);
        }

        if (mGatt == null || btManager.getConnectionState(mSpen, BluetoothProfile.GATT_SERVER)
                == BluetoothProfile.STATE_DISCONNECTED) {
            mGatt = mSpen.connectGatt(mContext, false,
                    new SPenGattCallback(this, mContext), BluetoothDevice.TRANSPORT_LE);
        }
    }

    public void disconnect() throws RemoteException {
        if (mGatt != null) {
            mGatt.disconnect();
            mGatt.close();
        }
        if (mSpen != null) {
            mSpen.disconnect();
        }

        mSPenHAL.setCharging(false);
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);

        if (mSpen != null && mSpen.getAddress().equals(device.getAddress())) {
            device.setPairingConfirmation(true);
            abortBroadcast(); // Prevent Settings from receiving the broadcast message
        }
    }

}
