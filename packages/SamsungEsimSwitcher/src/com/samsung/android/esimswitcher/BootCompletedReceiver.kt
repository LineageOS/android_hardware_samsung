/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package com.samsung.android.esimswitcher

import android.content.BroadcastReceiver
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.util.Log

class BootCompletedReceiver : BroadcastReceiver() {

    companion object {
        private const val TAG = "EsimSwitcher"
        private val DEBUG = Log.isLoggable(TAG, Log.DEBUG)
    }

    override fun onReceive(context: Context, intent: Intent) {
        if (DEBUG) Log.d(TAG, "Received boot completed intent: ${intent.action}")
        if (intent.action == Intent.ACTION_BOOT_COMPLETED) {
            val controller = EsimController.getInstance(context)
            val supported =
                controller.hasSlotSwitch() ||
                    context.resources
                        .getIntArray(com.android.internal.R.array.non_removable_euicc_slots)
                        .isNotEmpty()
            Log.i(TAG, "eSIM slot switch supported: $supported")

            setComponentEnabled(
                context,
                EsimSettingsActivity::class.java.name,
                supported,
            )

            if (supported) {
                controller.onBootCompleted()
            }
        }
    }

    private fun setComponentEnabled(context: Context, component: String, enabled: Boolean) {
        val name = ComponentName(context, component)
        val pm = context.packageManager
        val newState =
            if (enabled) {
                PackageManager.COMPONENT_ENABLED_STATE_ENABLED
            } else {
                PackageManager.COMPONENT_ENABLED_STATE_DISABLED
            }

        if (pm.getComponentEnabledSetting(name) != newState) {
            pm.setComponentEnabledSetting(name, newState, PackageManager.DONT_KILL_APP)
        }
    }
}
