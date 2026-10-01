/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package com.samsung.android.callmanager;

import android.net.Uri;
import android.telecom.CallRedirectionService;
import android.telecom.PhoneAccountHandle;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.media.AudioManager;
import android.util.Log;

public class SamsungCallRedirectionService extends CallRedirectionService {

    private static final String TAG = "SamsungCallRedirectionService";

    private TelephonyManager mTelephonyManager;
    private AudioManager mAudioManager;

    @Override
    public void onCreate() {
        super.onCreate();

        mTelephonyManager = getSystemService(TelephonyManager.class);
        mAudioManager = getSystemService(AudioManager.class);
    }

    private void setCallSimSlot(int slot) {
        int slotId = slot + 1;
        String parameter = "g_call_sim_slot=" + slotId;

        mAudioManager.setParameters(parameter);
        Log.i(TAG, "Passed audio parameter " + parameter);
    }

    @Override
    public void onPlaceCall(Uri handle, PhoneAccountHandle initialPhoneAccount, boolean allowInteractiveResponse) {
        try {
            int subId = mTelephonyManager.getSubscriptionId(initialPhoneAccount);

            if (subId == SubscriptionManager.INVALID_SUBSCRIPTION_ID) {
                Log.w(TAG, "Could not resolve subscription ID");
                return;
            }

            int slot = SubscriptionManager.getSlotIndex(subId);

            if (slot == SubscriptionManager.INVALID_SIM_SLOT_INDEX) {
                Log.w(TAG, "Could not resolve SIM slot " + slot + " for subId=" + subId);
                return;
            }

            // Outgoing phone calls. Pass the parameter before it starts to dial, otherwise the parameter is ignored
            setCallSimSlot(slot);

        } catch (SecurityException e) {
            Log.e(TAG, "Permission denied while resolving PhoneAccountHandle", e);
        } catch (RuntimeException e) {
            Log.e(TAG, "Failed to resolve outgoing SIM slot", e);
        } finally {
            // Continue with the phone call
            placeCallUnmodified();
        }
    }
}

