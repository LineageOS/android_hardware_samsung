/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package com.samsung.android.esimswitcher

import android.content.Context
import android.os.SystemProperties
import android.telephony.SubscriptionManager
import android.telephony.TelephonyManager
import android.telephony.UiccSlotInfo
import android.util.Log

/**
 * Drives Samsung tsds2 hybrid-slot switch via libsec-ril shim.
 *
 * Setting [PROP_ESIM_SWITCH] to "1"/"0" is bridged by init.esim_switch.rc into
 * vendor.calls.esim_switch. The RIL shim sends SEC_SIM_LOW_LEVEL_CONTROL and
 * synthesizes GetEID APDU from /efs/FactoryApp/eID so EuiccCard.mCardId is set
 * without framework patches. After the OEM flip we power-cycle physical slot 1
 * so telephony recreates EuiccCard and re-runs loadEid.
 */
class EsimController private constructor(private val context: Context) {

    companion object {
        private const val TAG = "EsimController"
        private val DEBUG = Log.isLoggable(TAG, Log.DEBUG)

        const val PROP_ESIM_SWITCH = "persist.sys.esim_switch"
        private const val PROP_ESIM_READY = "vendor.calls.esim_ready"
        private const val PROP_SIM_SLOT_TYPE_1 = "ril.simslottype1"
        private const val PROP_SIM_SLOT_TYPE_2 = "ril.simslottype2"
        private const val PROP_SLOT_SWITCH = "persist.ril.esim.slotswitch"

        // Hybrid eUICC is physical slot index 1 on tsds2.
        private const val EUICC_PHYSICAL_SLOT = 1

        private const val READY_TIMEOUT_MS = 90_000L
        private const val READY_POLL_MS = 200L
        private const val EID_WAIT_MS = 45_000L

        @Volatile private var instance: EsimController? = null

        fun getInstance(context: Context): EsimController {
            return instance
                ?: synchronized(this) {
                    instance ?: EsimController(context.applicationContext).also { instance = it }
                }
        }

        private fun isValidEid(value: String?): Boolean {
            if (value.isNullOrEmpty() || value.length != 32) return false
            return value.all { it.isDigit() || it in 'A'..'F' || it in 'a'..'f' }
        }
    }

    private val telephonyManager: TelephonyManager?
        get() = context.getSystemService(TelephonyManager::class.java)

    fun onBootCompleted() {
        if (DEBUG) Log.d(TAG, "onBootCompleted, enabled=${getEsimEnabled()}")
        if (getEsimEnabled() && isSlotTypeEsim()) {
            // Re-trigger EuiccCard loadEid after boot so shim GetEID synth runs.
            Thread({
                try {
                    refreshEuiccCardEid("boot")
                } catch (t: Throwable) {
                    Log.w(TAG, "boot eid refresh failed", t)
                }
            }, "esim-eid-boot").start()
        }
    }

    fun hasSlotSwitch(): Boolean {
        return SystemProperties.get(PROP_SLOT_SWITCH, "")
            .equals("tsds2", ignoreCase = true)
    }

    fun getEsimActive(): Boolean {
        val subscriptionManager =
            context.getSystemService(Context.TELEPHONY_SUBSCRIPTION_SERVICE) as? SubscriptionManager
        val subscriptionInfoList = subscriptionManager?.activeSubscriptionInfoList ?: return false

        for (subscriptionInfo in subscriptionInfoList) {
            if (subscriptionInfo.isEmbedded) {
                if (DEBUG)
                    Log.d(
                        TAG,
                        "Found eSIM profile: ${subscriptionInfo.displayName}, ${subscriptionInfo.carrierName}",
                    )
                return true
            }
        }
        if (DEBUG) Log.d(TAG, "No eSIM profiles found.")
        return false
    }

    /**
     * True when the hybrid tray (physical slot [EUICC_PHYSICAL_SLOT]) holds a present
     * physical SIM. Enabling eSIM would remux that slot to the built-in eUICC and
     * drop the pSIM — callers should ask the user to move it to the other slot first.
     */
    fun hasPhysicalSimInHybridSlot(): Boolean {
        if (isSlotTypeEsim()) return false
        val slots = telephonyManager?.uiccSlotsInfo ?: return false
        if (EUICC_PHYSICAL_SLOT >= slots.size) return false
        val slot = slots[EUICC_PHYSICAL_SLOT] ?: return false
        if (slot.cardStateInfo != UiccSlotInfo.CARD_STATE_INFO_PRESENT) return false
        if (slot.isEuicc) return false
        if (DEBUG) Log.d(TAG, "Physical SIM present in hybrid slot $EUICC_PHYSICAL_SLOT")
        return true
    }

    fun isSlotTypeEsim(): Boolean {
        val type1 = SystemProperties.get(PROP_SIM_SLOT_TYPE_1, "0")
        val type2 = SystemProperties.get(PROP_SIM_SLOT_TYPE_2, "0")
        return type1 == "1" || type2 == "1"
    }

    fun getEsimEnabled(): Boolean {
        return SystemProperties.get(PROP_ESIM_SWITCH, "0") == "1"
    }

    fun isEsimReady(): Boolean {
        return isSlotTypeEsim() &&
            SystemProperties.get(PROP_ESIM_READY, "0") == "1"
    }

    /** EID as exposed on UiccSlotInfo.cardId once EuiccCard.loadEid succeeds. */
    fun getEuiccSlotCardId(): String? {
        val slots = telephonyManager?.uiccSlotsInfo ?: return null
        if (EUICC_PHYSICAL_SLOT >= slots.size) return null
        val slot: UiccSlotInfo = slots[EUICC_PHYSICAL_SLOT] ?: return null
        val cardId = slot.cardId
        return if (isValidEid(cardId)) cardId else null
    }

    /**
     * Power-cycle the hybrid eUICC slot so EuiccCard is recreated and loadEid
     * runs against the shim's synthetic GetEID response.
     */
    private fun refreshEuiccCardEid(reason: String): Boolean {
        val tm = telephonyManager ?: return false
        Log.i(TAG, "refreshEuiccCardEid($reason)")
        try {
            tm.setSimPowerStateForSlot(EUICC_PHYSICAL_SLOT, TelephonyManager.CARD_POWER_DOWN)
        } catch (t: Throwable) {
            Log.w(TAG, "SIM power down failed", t)
            return false
        }
        try {
            Thread.sleep(1500)
        } catch (_: InterruptedException) {
            return false
        }
        try {
            tm.setSimPowerStateForSlot(EUICC_PHYSICAL_SLOT, TelephonyManager.CARD_POWER_UP)
        } catch (t: Throwable) {
            Log.w(TAG, "SIM power up failed", t)
            return false
        }

        val deadline = System.currentTimeMillis() + EID_WAIT_MS
        while (System.currentTimeMillis() < deadline) {
            val eid = getEuiccSlotCardId()
            if (eid != null) {
                Log.i(TAG, "EuiccSlot cardId/EID ready after $reason")
                return true
            }
            try {
                Thread.sleep(READY_POLL_MS)
            } catch (_: InterruptedException) {
                break
            }
        }
        Log.w(TAG, "EuiccSlot cardId still missing after $reason")
        return false
    }

    /**
     * Requests eSIM enable/disable and waits until hardware matches.
     * Returns true when slot type matches the request (or already matched).
     */
    fun setEsimEnabled(isEnabled: Boolean): Boolean {
        val value = if (isEnabled) "1" else "0"
        Log.i(
            TAG,
            "setEsimEnabled $isEnabled persist=${SystemProperties.get(PROP_ESIM_SWITCH, "")} " +
                "type2=${SystemProperties.get(PROP_SIM_SLOT_TYPE_2, "0")} " +
                "ready=${SystemProperties.get(PROP_ESIM_READY, "0")}",
        )

        SystemProperties.set(PROP_ESIM_SWITCH, value)

        if (isEnabled && isSlotTypeEsim()) {
            Log.i(TAG, "eSIM slot already active; refreshing EID")
            refreshEuiccCardEid("already-active")
            return true
        }
        if (!isEnabled && !isSlotTypeEsim()) {
            Log.i(TAG, "eSIM slot already inactive")
            return true
        }

        val deadline = System.currentTimeMillis() + READY_TIMEOUT_MS
        var flipped = false
        while (System.currentTimeMillis() < deadline) {
            if (SystemProperties.get(PROP_ESIM_SWITCH, "") != value) {
                Log.w(TAG, "persist changed during wait; aborting")
                return false
            }
            if (isEnabled && isSlotTypeEsim()) {
                Log.i(TAG, "eSIM enabled (simslottype*=1)")
                flipped = true
                break
            }
            if (!isEnabled && !isSlotTypeEsim()) {
                Log.i(TAG, "eSIM disabled")
                return true
            }
            try {
                Thread.sleep(READY_POLL_MS)
            } catch (_: InterruptedException) {
                break
            }
        }

        if (isEnabled && flipped) {
            refreshEuiccCardEid("after-switch")
            return true
        }

        val ok = if (isEnabled) isSlotTypeEsim() else !isSlotTypeEsim()
        Log.w(
            TAG,
            "setEsimEnabled wait finished ok=$ok type2=${SystemProperties.get(PROP_SIM_SLOT_TYPE_2, "0")} " +
                "ready=${SystemProperties.get(PROP_ESIM_READY, "0")} eid=${getEuiccSlotCardId()}",
        )
        return ok
    }
}
