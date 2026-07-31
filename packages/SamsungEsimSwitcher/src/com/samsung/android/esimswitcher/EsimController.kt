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
        private const val EID_WAIT_MS = 15_000L
        private const val SIM_POWER_CYCLE_MS = 400L

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
     *
     * After remux to pSIM with an empty tray, RIL often still reports slot 1 as
     * PRESENT with isEuicc=false, isRemovable=false and an empty/EID cardId. That
     * is built-in eUICC residue, not a tray SIM — do not block on it.
     */
    fun hasPhysicalSimInHybridSlot(): Boolean {
        if (isSlotTypeEsim()) return false
        val slots = telephonyManager?.uiccSlotsInfo ?: return false
        if (EUICC_PHYSICAL_SLOT >= slots.size) return false
        val slot = slots[EUICC_PHYSICAL_SLOT] ?: return false
        if (slot.cardStateInfo != UiccSlotInfo.CARD_STATE_INFO_PRESENT) return false
        if (slot.isEuicc) return false
        // Tray pSIM is removable; built-in eUICC residue is not.
        if (!slot.isRemovable) return false
        val cardId = slot.cardId
        // Real pSIM has an ICCID; empty or EID-shaped ids are not tray SIMs.
        if (cardId.isNullOrEmpty() || isValidEid(cardId)) return false
        if (DEBUG) {
            Log.d(
                TAG,
                "Physical SIM present in hybrid slot $EUICC_PHYSICAL_SLOT cardId=$cardId",
            )
        }
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

    private fun syncPersistToHardware() {
        val want = if (isSlotTypeEsim()) "1" else "0"
        if (SystemProperties.get(PROP_ESIM_SWITCH, "") != want) {
            Log.w(TAG, "reverting $PROP_ESIM_SWITCH to $want (matches hardware)")
            SystemProperties.set(PROP_ESIM_SWITCH, want)
        }
    }

    private fun maybeRefreshEidAsync(reason: String) {
        if (getEuiccSlotCardId() != null) {
            Log.i(TAG, "EID already present; skip refresh ($reason)")
            return
        }
        Thread({
            try {
                refreshEuiccCardEid(reason)
            } catch (t: Throwable) {
                Log.w(TAG, "async eid refresh failed ($reason)", t)
            }
        }, "esim-eid-$reason").start()
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
            Thread.sleep(SIM_POWER_CYCLE_MS)
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
     * On failure, persist is reverted to match hardware so the UI cannot lie.
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
            Log.i(TAG, "eSIM slot already active")
            maybeRefreshEidAsync("already-active")
            return true
        }
        if (!isEnabled && !isSlotTypeEsim()) {
            Log.i(TAG, "eSIM slot already inactive")
            return true
        }

        val deadline = System.currentTimeMillis() + READY_TIMEOUT_MS
        while (System.currentTimeMillis() < deadline) {
            if (SystemProperties.get(PROP_ESIM_SWITCH, "") != value) {
                Log.w(TAG, "persist changed during wait; aborting")
                syncPersistToHardware()
                return false
            }
            if (isEnabled && isEsimReady()) {
                Log.i(TAG, "eSIM enabled (type + ready)")
                maybeRefreshEidAsync("after-switch")
                return true
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

        val ok = if (isEnabled) isEsimReady() else !isSlotTypeEsim()
        if (!ok) {
            syncPersistToHardware()
        }
        Log.w(
            TAG,
            "setEsimEnabled wait finished ok=$ok type2=${SystemProperties.get(PROP_SIM_SLOT_TYPE_2, "0")} " +
                "ready=${SystemProperties.get(PROP_ESIM_READY, "0")} eid=${getEuiccSlotCardId()}",
        )
        return ok
    }
}
