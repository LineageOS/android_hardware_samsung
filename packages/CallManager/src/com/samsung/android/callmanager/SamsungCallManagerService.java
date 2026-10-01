/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package com.samsung.android.callmanager;

import android.app.Service;
import android.content.Intent;
import android.media.AudioManager;
import android.os.IBinder;
import android.telephony.Annotation.CallState;
import android.telephony.SubscriptionInfo;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyCallback;
import android.telephony.TelephonyManager;
import android.util.Log;
import android.app.role.RoleManager;
import android.os.UserHandle;
import java.util.function.Consumer;

public class SamsungCallManagerService extends Service {

    private static final String TAG = "SamsungCallManagerService";

    private TelephonyManager[] mTelephonyManagers;
    private CallStateCallback[] mCallStateCallbacks;
    private TelephonyManager mTelephonyManager;

    private SubscriptionManager mSubscriptionManager;
    private SubscriptionManager.OnSubscriptionsChangedListener mSubscriptionListener;

    private SamsungCallUtils mSamsungCallUtils;

    @Override
    public void onCreate() {
        super.onCreate();

        setupCallRedirectionRole();

        mSubscriptionManager = getSystemService(SubscriptionManager.class);
        mTelephonyManager = getSystemService(TelephonyManager.class);

        AudioManager audioManager = getSystemService(AudioManager.class);
        mSamsungCallUtils = new SamsungCallUtils(audioManager);

        int slotCount = mTelephonyManager.getPhoneCount();

        mTelephonyManagers = new TelephonyManager[slotCount];
        mCallStateCallbacks = new CallStateCallback[slotCount];

        mSubscriptionListener = new SubscriptionManager.OnSubscriptionsChangedListener() {
            @Override
            public void onSubscriptionsChanged() {
                updateSubscriptions();
            }
        };

        mSubscriptionManager.addOnSubscriptionsChangedListener(getMainExecutor(), mSubscriptionListener);

        updateSubscriptions();
    }

    private void setupCallRedirectionRole() {
        RoleManager roleManager = getSystemService(RoleManager.class);

        if (roleManager.isRoleHeld(RoleManager.ROLE_CALL_REDIRECTION)) {
            Log.i(TAG, "CALL_REDIRECTION role already held");
            return;
        }

        Log.i(TAG, "Requesting CALL_REDIRECTION role");

        try {
            roleManager.addRoleHolderAsUser(
                    RoleManager.ROLE_CALL_REDIRECTION,
                    getPackageName(),
                    0,
                    UserHandle.of(UserHandle.USER_SYSTEM),
                    getMainExecutor(),
                    new Consumer<Boolean>() {
                        @Override
                        public void accept(Boolean successful) {
                            Log.i(TAG, "CALL_REDIRECTION role result: " + successful);
                        }
                    }
            );
        } catch (SecurityException e) {
            Log.e(TAG, "No permission to manage role holders", e);
        } catch (RuntimeException e) {
            Log.e(TAG, "Failed to request CALL_REDIRECTION role", e);
        }
    }

    private void updateSubscriptions() {
        // Unregister existing callbacks before registering new ones
        unregisterTelephonyCallbacks();

        int slotCount = mTelephonyManager.getPhoneCount();

        for (int slot = 0; slot < slotCount; slot++) {
            SubscriptionInfo subscriptionInfo = mSubscriptionManager.getActiveSubscriptionInfoForSimSlotIndex(slot);

            // Skip empty SIM slots
            if (subscriptionInfo == null) {
                Log.d(TAG, "slot" + (slot + 1) + " not active");
                continue;
            }

            TelephonyManager mSlotManager = mTelephonyManager.createForSubscriptionId(subscriptionInfo.getSubscriptionId());
            CallStateCallback callStateCallback = new CallStateCallback(slot);

            mTelephonyManagers[slot] = mSlotManager;
            mCallStateCallbacks[slot] = callStateCallback;

            mSlotManager.registerTelephonyCallback(getMainExecutor(), callStateCallback);

            Log.d(TAG, "Watching slot" + (slot + 1));
        }
    }

    private void unregisterTelephonyCallbacks() {
        if (mTelephonyManagers == null || mCallStateCallbacks == null) {
            return;
        }

        for (int slot = 0; slot < mTelephonyManagers.length; slot++) {
            if (mTelephonyManagers[slot] != null && mCallStateCallbacks[slot] != null) {
                mTelephonyManagers[slot].unregisterTelephonyCallback(mCallStateCallbacks[slot]);
                mTelephonyManagers[slot] = null;
                mCallStateCallbacks[slot] = null;
            }
        }
    }

    private class CallStateCallback extends TelephonyCallback implements TelephonyCallback.CallStateListener {
        private final int slot;

        CallStateCallback(int slot) {
            this.slot = slot;
        }

        @Override
        public void onCallStateChanged(@CallState int state) {
            // Incoming phone calls. Set parameter as soon as it starts to ring
            if (state == TelephonyManager.CALL_STATE_RINGING) {
                mSamsungCallUtils.setCallSimSlot(slot);
            }
        }
    }

    @Override
    public void onDestroy() {
        if (mSubscriptionManager != null && mSubscriptionListener != null) {
            mSubscriptionManager.removeOnSubscriptionsChangedListener(mSubscriptionListener);
        }

        unregisterTelephonyCallbacks();

        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
