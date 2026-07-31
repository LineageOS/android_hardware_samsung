/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package com.samsung.android.esimswitcher

import android.os.Bundle
import android.util.Log
import androidx.appcompat.app.AlertDialog
import androidx.preference.Preference
import com.android.settingslib.widget.FooterPreference
import com.android.settingslib.widget.MainSwitchPreference
import com.android.settingslib.widget.SettingsBasePreferenceFragment

class EsimSettingsFragment :
    SettingsBasePreferenceFragment(), Preference.OnPreferenceChangeListener {

    companion object {
        private const val TAG = "EsimSettingsFragment"
        private val DEBUG = Log.isLoggable(TAG, Log.DEBUG)
    }

    private val esimController by lazy { EsimController.getInstance(requireContext()) }

    private val switchBar by lazy { findPreference<MainSwitchPreference>("esim_enable")!! }
    private val footerPref by lazy { findPreference<FooterPreference>("esim_footer")!! }

    private val switchCallback = EsimController.SwitchCallback { ok -> onSwitchFinished(ok) }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        if (DEBUG) Log.d(TAG, "onCreatePreferences")
        setPreferencesFromResource(R.xml.settings_esim, rootKey)

        switchBar.onPreferenceChangeListener = this
        syncUi()
    }

    override fun onResume() {
        super.onResume()
        esimController.setSwitchCallback(switchCallback)
        // A switch started earlier may still be running: restore that state
        // rather than showing an idle switch the user could fight with.
        syncUi()
    }

    override fun onPause() {
        esimController.setSwitchCallback(null)
        super.onPause()
    }

    private fun syncUi() {
        val running = esimController.isSwitchInProgress()
        switchBar.isChecked = esimController.getEsimEnabled()
        switchBar.isEnabled = !running
        footerPref.title =
            getString(if (running) R.string.esim_switching else R.string.esim_footer_note)
    }

    private fun onSwitchFinished(ok: Boolean) {
        if (!isAdded) return
        if (DEBUG) Log.d(TAG, "onSwitchFinished: $ok")
        switchBar.isEnabled = true
        switchBar.isChecked = esimController.getEsimEnabled()
        footerPref.title =
            getString(if (ok) R.string.esim_footer_note else R.string.esim_switch_failed)
    }

    override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {
        if (preference == switchBar) {
            val isChecked = newValue as Boolean
            if (DEBUG) Log.d(TAG, "onPreferenceChange: $isChecked")
            if (esimController.getEsimActive()) {
                if (isChecked) {
                    // Already carrying an active eSIM profile, so there is
                    // nothing to enable; resync in case persist drifted from
                    // the subscription state.
                    Log.w(TAG, "eSIM already active; ignoring enable request")
                    switchBar.isChecked = true
                    return false
                }
                AlertDialog.Builder(requireContext())
                    .setTitle(R.string.esim_warning_title)
                    .setMessage(R.string.esim_warning_message)
                    .setPositiveButton(android.R.string.ok, null)
                    .setCancelable(false)
                    .show()
                return false
            }

            if (isChecked && esimController.hasPhysicalSimInHybridSlot()) {
                AlertDialog.Builder(requireContext())
                    .setTitle(R.string.esim_psim_slot_title)
                    .setMessage(R.string.esim_psim_slot_message)
                    .setNegativeButton(android.R.string.cancel, null)
                    .setPositiveButton(R.string.esim_psim_slot_proceed) { _, _ ->
                        if (!isAdded) return@setPositiveButton
                        startEsimSwitch(true)
                    }
                    .setCancelable(true)
                    .show()
                // Keep the switch off until the user chooses Proceed.
                return false
            }

            startEsimSwitch(isChecked)
            // Optimistic UI; corrected when background work finishes.
            return true
        }
        return true
    }

    private fun startEsimSwitch(isChecked: Boolean) {
        if (!esimController.requestEsimEnabled(isChecked)) {
            syncUi()
            return
        }
        switchBar.isChecked = isChecked
        switchBar.isEnabled = false
        footerPref.title = getString(R.string.esim_switching)
    }
}
