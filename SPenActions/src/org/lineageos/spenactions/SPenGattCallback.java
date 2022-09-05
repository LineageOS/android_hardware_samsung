/*
 * SPDX-FileCopyrightText: 2021-2022 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.spenactions;

import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothProfile;
import android.content.Context;
import android.hardware.input.InputManager;
import android.os.RemoteException;
import android.os.SystemClock;
import android.util.Log;
import android.view.InputDevice;
import android.view.InputEvent;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;

import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

import org.lineageos.spenactions.settings.SettingsUtils;

public class SPenGattCallback extends BluetoothGattCallback {

    private static final String LOG_TAG = "SPenActions/SPenGattCallback";

    // GATT characteristics UUIDs
    private static final UUID BATTERY_LEVEL = UUID.fromString("5a87b4ef-3bfa-76a8-e642-92933c31434f");
    private static final UUID BUTTON_EVENT = UUID.fromString("6c290d2e-1c03-aca1-ab48-a9b908bae79e");
    private static final UUID CHARGE_STATUS = UUID.fromString("92933c31-41d8-bda6-3c31-434fab48a9b9");
    private static final UUID PEN_LOG = UUID.fromString("fe3c10ee-16dd-4b73-9a37-e1e6024a3848");
    private static final UUID RAW_SENSOR_DATA = UUID.fromString("ddb42396-ca00-4db3-b87d-2ee458279360");
    private static final UUID SELF_TEST = UUID.fromString("8bd867d3-d619-45d9-8ee0-3814dbd5b3f0");

    private static final String CLIENT_CHARACTERISTIC_CONFIG = "00002902-0000-1000-8000-00805f9b34fb";

    private final SPenConnectionManager mConnectionManager;
    private final Context mContext;
    private final UUID mServiceUUID;
    private final List<UUID> mEnableCharacteristics;
    private long mButtonDownTime = 0;
    private boolean mButtonHadGesture = false;

    public SPenGattCallback(SPenConnectionManager connectionManager, Context context) {
        this.mConnectionManager = connectionManager;
        this.mContext = context;
        this.mServiceUUID = UUID.fromString(
                context.getResources().getString(R.string.config_sPenServiceUuid));
        this.mEnableCharacteristics = new ArrayList<>();

        mEnableCharacteristics.add(BATTERY_LEVEL);
        mEnableCharacteristics.add(BUTTON_EVENT);
    }

    @Override
    public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
        super.onConnectionStateChange(gatt, status, newState);

        if (newState == BluetoothProfile.STATE_CONNECTED) {
            Log.i(LOG_TAG, "SPen connected!");
            mConnectionManager.onConnected();
            gatt.discoverServices();
        } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
            gatt.disconnect();
            gatt.close();
            Log.e(LOG_TAG, "SPen disconnected!");

            try {
                mConnectionManager.connect();
            } catch (RemoteException ex) {
                ex.printStackTrace();
            }
        }
    }

    @Override
    public void onServicesDiscovered(BluetoothGatt gatt, int status) {
        super.onServicesDiscovered(gatt, status);

        BluetoothGattService service = gatt.getService(mServiceUUID);
        if (service != null) {
            Log.i(LOG_TAG, "Found SPen GATT service: " + mServiceUUID);
            if (mEnableCharacteristics.size() > 0) {
                Log.i(LOG_TAG, "Enabling characteristic notifications");
                setCharacteristicNotification(gatt,
                        service.getCharacteristic(mEnableCharacteristics.get(0)), true);
            }
        } else {
            Log.e(LOG_TAG, "Unable to find SPen GATT service!");
        }
    }

    @Override
    public void onDescriptorWrite(BluetoothGatt gatt,
            BluetoothGattDescriptor descriptor, int status) {
        UUID uuid = descriptor.getCharacteristic().getUuid();
        int index = mEnableCharacteristics.indexOf(uuid);

        if (index >= 0 && ++index < mEnableCharacteristics.size()) {
            BluetoothGattService service = gatt.getService(mServiceUUID);
            // GATT doesn't support multiple operations at the same time,
            // so continue here once previous operation is done
            setCharacteristicNotification(gatt,
                    service.getCharacteristic(mEnableCharacteristics.get(index)), true);
        }
    }

    @Override
    public void onCharacteristicChanged(BluetoothGatt gatt,
            BluetoothGattCharacteristic characteristic) {
        super.onCharacteristicChanged(gatt, characteristic);

        if (characteristic.getUuid().equals(BUTTON_EVENT)) {
            int type = characteristic.getValue()[0] & 255;
            ButtonAction button = ButtonAction.fromType(type);

            if (button != null) {
                switch (button.getAction()) {
                    case UP:
                        if (!mButtonHadGesture) {
                            SPenMode mode = SPenMode.valueOfPref(SettingsUtils.getSwitchPreference(
                                    mContext, SettingsUtils.SPEN_MODE, "0"));
                            // Can be used for e.g taking photos
                            sendEvent(mButtonDownTime, KeyEvent.ACTION_DOWN, mode.getButtonKey());
                            sendEvent(KeyEvent.ACTION_UP, mode.getButtonKey());
                        }
                        mButtonHadGesture = false;
                        break;
                    case DOWN:
                        mButtonDownTime = SystemClock.uptimeMillis();
                        break;
                    default:
                        break;
                }
                return;
            }

            MotionEvent move = MotionEvent.fromTypeData(type, characteristic.getValue());

            if (move != null) {
                if (Math.abs(move.getDX()) > 500 || Math.abs(move.getDY()) > 500) {
                    SPenMode mode = SPenMode.valueOfPref(SettingsUtils.getSwitchPreference(
                            mContext, SettingsUtils.SPEN_MODE, "0"));
                    if (Math.abs(move.getDX()) > Math.abs(move.getDY())) { // right/left
                        sendEvent(KeyEvent.ACTION_DOWN, mode.getGestureKey(move.getDX() > 0 ?
                                SPenDirection.POSITIVE_X : SPenDirection.NEGATIVE_X));
                        sendEvent(KeyEvent.ACTION_UP, mode.getGestureKey(move.getDX() > 0 ?
                                SPenDirection.POSITIVE_X : SPenDirection.NEGATIVE_X));
                    } else { // up/down NOTE: negative Y is actually up while positive Y is down!
                        sendEvent(KeyEvent.ACTION_DOWN, mode.getGestureKey(move.getDY() > 0 ?
                                SPenDirection.POSITIVE_Y : SPenDirection.NEGATIVE_Y));
                        sendEvent(KeyEvent.ACTION_UP, mode.getGestureKey(move.getDY() > 0 ?
                                SPenDirection.POSITIVE_Y : SPenDirection.NEGATIVE_Y));
                    }
                    mButtonHadGesture = true;
                }
            }
        } else if (characteristic.getUuid().equals(BATTERY_LEVEL)) {
            Log.i(LOG_TAG, "Battery level: " + characteristic.getValue()[0]);
        } else {
            Log.e(LOG_TAG, "Unknown characteristic: " + characteristic.getUuid());
        }
    }

    private void setCharacteristicNotification(BluetoothGatt gatt,
            BluetoothGattCharacteristic characteristic, boolean enable) {
        gatt.setCharacteristicNotification(characteristic, enable);
        UUID uuid = UUID.fromString(CLIENT_CHARACTERISTIC_CONFIG);
        BluetoothGattDescriptor descriptor = characteristic.getDescriptor(uuid);
        if (descriptor != null) {
            descriptor.setValue(enable
                    ? BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                    : BluetoothGattDescriptor.DISABLE_NOTIFICATION_VALUE);
            gatt.writeDescriptor(descriptor);
        }
    }

    private boolean sendEvent(long when, int action, int code) {
        final KeyEvent ev = new KeyEvent(when, when, action, code, 0 /* repeat */,
                0 /* metaState */, KeyCharacterMap.VIRTUAL_KEYBOARD, 0 /* scancode */,
                KeyEvent.FLAG_FROM_SYSTEM | KeyEvent.FLAG_VIRTUAL_HARD_KEY,
                InputDevice.SOURCE_KEYBOARD);

        ev.setDisplayId(mContext.getDisplay().getDisplayId());
        return InputManager.getInstance()
                .injectInputEvent(ev, InputManager.INJECT_INPUT_EVENT_MODE_ASYNC);
    }

    private boolean sendEvent(int action, int code) {
        return sendEvent(SystemClock.uptimeMillis(), action, code);
    }
}
