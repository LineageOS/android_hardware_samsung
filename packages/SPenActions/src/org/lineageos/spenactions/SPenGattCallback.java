/*
 * SPDX-FileCopyrightText: 2021-2025 The LineageOS Project
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

import org.lineageos.spenactions.settings.SettingsUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

public class SPenGattCallback extends BluetoothGattCallback {

    private static final String LOG_TAG = "SPenActions/SPenGattCallback";

    private static final int MOTION_THRESHOLD = 500;

    // GATT characteristics UUIDs
    private static final UUID BATTERY_LEVEL = UUID.fromString("5a87b4ef-3bfa-76a8-e642-92933c31434f");
    private static final UUID BUTTON_EVENT = UUID.fromString("6c290d2e-1c03-aca1-ab48-a9b908bae79e");
    private static final UUID CHARGE_STATUS = UUID.fromString("92933c31-41d8-bda6-3c31-434fab48a9b9");
    private static final UUID PEN_LOG = UUID.fromString("fe3c10ee-16dd-4b73-9a37-e1e6024a3848");
    private static final UUID RAW_SENSOR_DATA = UUID.fromString("ddb42396-ca00-4db3-b87d-2ee458279360");
    private static final UUID SELF_TEST = UUID.fromString("8bd867d3-d619-45d9-8ee0-3814dbd5b3f0");

    private static final String CLIENT_CHARACTERISTIC_CONFIG = "00002902-0000-1000-8000-00805f9b34fb";

    private final Context mContext;
    private final SPenConnectionManager mConnectionManager;
    private final List<UUID> mEnableCharacteristics = new ArrayList<>();
    private final UUID mServiceUUID;

    private boolean mButtonHadGesture = false;
    private long mButtonDownTime = 0;

    public SPenGattCallback(SPenConnectionManager connectionManager, Context context) {
        mConnectionManager = connectionManager;
        mContext = context;

        mEnableCharacteristics.add(BATTERY_LEVEL);
        mEnableCharacteristics.add(BUTTON_EVENT);

        mServiceUUID = UUID.fromString(
                context.getResources().getString(R.string.config_sPenServiceUuid));
    }

    @Override
    public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
        if (newState == BluetoothProfile.STATE_CONNECTED) {
            Log.i(LOG_TAG, "S Pen connected");
            mConnectionManager.onConnected();
            gatt.discoverServices();
        } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
            gatt.disconnect();
            gatt.close();
            Log.i(LOG_TAG, "S Pen disconnected");

            try {
                mConnectionManager.connect();
            } catch (RemoteException ex) {
                ex.printStackTrace();
            }
        }
    }

    @Override
    public void onServicesDiscovered(BluetoothGatt gatt, int status) {
        BluetoothGattService service = gatt.getService(mServiceUUID);
        if (service != null) {
            Log.i(LOG_TAG, "Found S Pen GATT service: " + mServiceUUID);
            if (mEnableCharacteristics.size() > 0) {
                Log.i(LOG_TAG, "Enabling characteristic notifications");
                setCharacteristicNotification(gatt,
                        service.getCharacteristic(mEnableCharacteristics.get(0)), true);
            }
        } else {
            Log.e(LOG_TAG, "Unable to find S Pen GATT service!");
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
        if (characteristic.getUuid().equals(BUTTON_EVENT)) {
            handleButtonEvent(characteristic.getValue());
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

    private void handleButtonEvent(byte[] data) {
        int type = data[0] & 0xFF;

        ButtonAction button = ButtonAction.fromType(type);
        if (button != null) {
            switch (button.getAction()) {
                case DOWN:
                    mButtonDownTime = SystemClock.uptimeMillis();
                    break;
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
                default:
                    break;
            }
            return;
        }

        MotionEvent move = MotionEvent.fromTypeData(type, data);
        handleGesture(move);
    }

    private void handleGesture(MotionEvent move) {
        if (move == null) {
            return;
        }

        int key;
        short dx = move.getDX();
        short dy = move.getDY();

        if (Math.abs(dx) <= MOTION_THRESHOLD && Math.abs(dy) <= MOTION_THRESHOLD) {
            return;
        }

        SPenMode mode = SPenMode.valueOfPref(SettingsUtils.getSwitchPreference(
                mContext, SettingsUtils.SPEN_MODE, "0"));

        if (Math.abs(dx) > Math.abs(dy)) { // right/left
            key = mode.getGestureKey(dx > 0
                    ? SPenDirection.POSITIVE_X
                    : SPenDirection.NEGATIVE_X);
        } else { // up/down (NOTE: negative Y is actually up while positive Y is down!)
            key = mode.getGestureKey(dy > 0
                    ? SPenDirection.POSITIVE_Y
                    : SPenDirection.NEGATIVE_Y);
        }

        sendEvent(KeyEvent.ACTION_DOWN, key);
        sendEvent(KeyEvent.ACTION_UP, key);
        mButtonHadGesture = true;
    }

    private boolean sendEvent(long when, int action, int code) {
        final KeyEvent ev = new KeyEvent(when, when, action, code, 0 /* repeat */,
                0 /* metaState */, KeyCharacterMap.VIRTUAL_KEYBOARD, 0 /* scancode */,
                KeyEvent.FLAG_FROM_SYSTEM | KeyEvent.FLAG_VIRTUAL_HARD_KEY,
                InputDevice.SOURCE_KEYBOARD);

        ev.setDisplayId(mContext.getDisplay().getDisplayId());
        return mContext.getSystemService(InputManager.class)
                .injectInputEvent(ev, InputManager.INJECT_INPUT_EVENT_MODE_ASYNC);
    }

    private boolean sendEvent(int action, int code) {
        return sendEvent(SystemClock.uptimeMillis(), action, code);
    }
}
