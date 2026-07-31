/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.settings.esimswitcher

import android.content.Context
import android.os.Handler
import android.os.Looper
import android.telephony.TelephonyManager
import android.telephony.UiccSlotInfo
import android.telephony.UiccSlotMapping
import android.util.Log
import java.util.concurrent.Executors
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

    private fun setEsimEnabled(enable: Boolean): Boolean {
        val tm = telephonyManager ?: return false
        if (isEsimEnabled() == enable) {
            Log.i(TAG, "second SIM already ${if (enable) "eSIM" else "pSIM"}")
            return true
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
