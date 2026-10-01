/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package com.samsung.android.callmanager;

import android.media.AudioManager;
import android.util.Log;

public class SamsungCallUtils {

    private static final String TAG = "SamsungCallUtils";

    private final AudioManager mAudioManager;

    public SamsungCallUtils(AudioManager audioManager) {
        mAudioManager = audioManager;
    }

    public void setCallSimSlot(int slot) {
        int slotId = slot + 1;
        String parameter = "g_call_sim_slot=" + slotId;

        mAudioManager.setParameters(parameter);
        Log.i(TAG, "Passed audio parameter " + parameter);
    }
}
