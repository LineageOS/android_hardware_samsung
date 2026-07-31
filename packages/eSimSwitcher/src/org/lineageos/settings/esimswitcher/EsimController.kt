/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.settings.esimswitcher

import android.Manifest
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Handler
import android.os.Looper
import android.telephony.SubscriptionManager
import android.telephony.TelephonyManager
import android.telephony.UiccSlotInfo
import android.telephony.UiccSlotMapping
import android.telephony.euicc.EuiccManager
import android.util.Log
import java.util.concurrent.CompletableFuture
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import java.util.concurrent.TimeoutException
import java.util.concurrent.atomic.AtomicBoolean

/**
 * Switches Samsung's tsds2 hybrid SIM path between the second tray and the eUICC. The radio HAL
 * reports both as separate physical slots, so Settings handles SIMs it already knows; this only
 * covers switching to the eUICC to download a profile, and to a SIM the HAL has not seen yet.
 */
class EsimController private constructor(private val context: Context) {

    /** Notified on the main thread when a switch requested earlier finishes. */
    fun interface SwitchCallback {
        fun onSwitchFinished(ok: Boolean, esimEnabled: Boolean)
    }

    companion object {
        private const val TAG = "EsimController"

        // The radio HAL's physical slots on tsds2: 0 is the first tray, 1 the eUICC and 2 the
        // second tray. Logical slot 1 moves between the last two.
        private const val EUICC_SLOT = 1
        private const val HYBRID_TRAY_SLOT = 2

        private val MAPPING_PSIM =
            listOf(UiccSlotMapping(0, 0, 0), UiccSlotMapping(0, HYBRID_TRAY_SLOT, 1))
        private val MAPPING_ESIM =
            listOf(UiccSlotMapping(0, 0, 0), UiccSlotMapping(0, EUICC_SLOT, 1))

        private const val ACTION_DISABLE_RESULT =
            "org.lineageos.settings.esimswitcher.action.DISABLE_RESULT"
        // The LPA waits up to 30 s for the SIM state after disabling a profile.
        private const val DISABLE_TIMEOUT_SECONDS = 45L

        @Volatile private var instance: EsimController? = null

        fun getInstance(context: Context): EsimController {
            return instance
                ?: synchronized(this) {
                    instance ?: EsimController(context.applicationContext).also { instance = it }
                }
        }
    }

    private val telephonyManager: TelephonyManager?
        get() = context.getSystemService(TelephonyManager::class.java)

    private val mainHandler = Handler(Looper.getMainLooper())

    // Owned by the singleton, not by the UI, so a switch finishes even if the user leaves
    // Settings.
    private val switchExecutor =
        Executors.newSingleThreadExecutor { runnable -> Thread(runnable, "esim-switch") }
    private val switchInFlight = AtomicBoolean(false)

    @Volatile private var switchCallback: SwitchCallback? = null

    fun isSwitchInProgress(): Boolean = switchInFlight.get()

    /** Registers the observer for an in-flight switch; pass null to detach. */
    fun setSwitchCallback(callback: SwitchCallback?) {
        switchCallback = callback
    }

    /**
     * Starts a switch on a background thread owned by this singleton and returns whether it was
     * accepted (false when one is already running).
     */
    fun requestEsimEnabled(enable: Boolean): Boolean {
        if (!switchInFlight.compareAndSet(false, true)) {
            Log.w(TAG, "switch already in progress; ignoring request $enable")
            return false
        }
        switchExecutor.execute {
            val ok =
                try {
                    setEsimEnabled(enable)
                } finally {
                    switchInFlight.set(false)
                }
            // The framework updates the mapping only once the new slot status arrives, so
            // report the requested state on success rather than reading it back.
            val enabled = if (ok) enable else isEsimEnabled()
            mainHandler.post { switchCallback?.onSwitchFinished(ok, enabled) }
        }
        return true
    }

    fun isEsimEnabled(): Boolean =
        telephonyManager?.simSlotMapping?.any {
            it.logicalSlotIndex == 1 && it.physicalSlotIndex == EUICC_SLOT
        } ?: false

    /**
     * True when the second tray holds a card. Enabling eSIM switches that path to the eUICC and
     * drops the SIM, so callers should warn before proceeding.
     */
    fun hasPhysicalSimInHybridSlot(): Boolean {
        if (isEsimEnabled()) return false
        val slot = telephonyManager?.uiccSlotsInfo?.getOrNull(HYBRID_TRAY_SLOT) ?: return false
        return slot.cardStateInfo == UiccSlotInfo.CARD_STATE_INFO_PRESENT
    }

    /**
     * Disables the profiles on the eUICC's active ports, as Settings does before it disconnects
     * one. A profile left enabled on a disconnected port cannot be enabled on the other port, and
     * the LPA cannot switch the eUICC back in to disable it.
     */
    private fun disableActiveProfiles(tm: TelephonyManager): Boolean {
        val ports =
            tm.uiccSlotsInfo?.getOrNull(EUICC_SLOT)?.ports?.filter {
                it.isActive && !it.iccId.isNullOrEmpty()
            } ?: return true
        val euiccManager = context.getSystemService(EuiccManager::class.java) ?: return false
        return ports.all { disableProfile(euiccManager, it.portIndex) }
    }

    private fun disableProfile(euiccManager: EuiccManager, port: Int): Boolean {
        val result = CompletableFuture<Int>()
        val receiver =
            object : BroadcastReceiver() {
                override fun onReceive(context: Context, intent: Intent) {
                    result.complete(resultCode)
                }
            }
        context.registerReceiver(
            receiver,
            IntentFilter(ACTION_DISABLE_RESULT),
            Manifest.permission.WRITE_EMBEDDED_SUBSCRIPTIONS,
            null,
            Context.RECEIVER_EXPORTED,
        )
        return try {
            val callback =
                PendingIntent.getBroadcast(
                    context,
                    port,
                    Intent(ACTION_DISABLE_RESULT).setPackage(context.packageName),
                    PendingIntent.FLAG_CANCEL_CURRENT or PendingIntent.FLAG_IMMUTABLE,
                )
            euiccManager.switchToSubscription(
                SubscriptionManager.INVALID_SUBSCRIPTION_ID,
                port,
                callback,
            )
            val code = result.get(DISABLE_TIMEOUT_SECONDS, TimeUnit.SECONDS)
            Log.i(TAG, "disabling the profile on port $port returned $code")
            code == EuiccManager.EMBEDDED_SUBSCRIPTION_RESULT_OK
        } catch (e: TimeoutException) {
            Log.e(TAG, "disabling the profile on port $port timed out")
            false
        } finally {
            context.unregisterReceiver(receiver)
        }
    }

    private fun setEsimEnabled(enable: Boolean): Boolean {
        val tm = telephonyManager ?: return false
        if (isEsimEnabled() == enable) {
            Log.i(TAG, "second SIM already ${if (enable) "eSIM" else "pSIM"}")
            return true
        }
        if (!enable && !disableActiveProfiles(tm)) {
            return false
        }
        val mapping = if (enable) MAPPING_ESIM else MAPPING_PSIM
        return try {
            // Blocks until the RIL answers; throws if the request is rejected.
            tm.setSimSlotMapping(mapping)
            Log.i(TAG, "setSimSlotMapping($mapping) done")
            true
        } catch (e: RuntimeException) {
            Log.e(TAG, "setSimSlotMapping($mapping) failed", e)
            false
        }
    }
}
