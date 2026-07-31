/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package com.samsung.android.esimswitcher

import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import androidx.appcompat.app.AlertDialog
import androidx.preference.Preference
import com.android.settingslib.widget.FooterPreference
import com.android.settingslib.widget.MainSwitchPreference
import com.android.settingslib.widget.SettingsBasePreferenceFragment
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicInteger

class EsimSettingsFragment :
    SettingsBasePreferenceFragment(), Preference.OnPreferenceChangeListener {

    companion object {
        private const val TAG = "EsimSettingsFragment"
        private val DEBUG = Log.isLoggable(TAG, Log.DEBUG)
    }

    private val esimController by lazy { EsimController.getInstance(requireContext()) }
    private val executor = Executors.newSingleThreadExecutor()
    private val mainHandler = Handler(Looper.getMainLooper())
    private val requestGen = AtomicInteger(0)

    private val switchBar by lazy { findPreference<MainSwitchPreference>("esim_enable")!! }
    private val footerPref by lazy { findPreference<FooterPreference>("esim_footer")!! }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        if (DEBUG) Log.d(TAG, "onCreatePreferences")
        setPreferencesFromResource(R.xml.settings_esim, rootKey)

        switchBar.onPreferenceChangeListener = this
        switchBar.isChecked = esimController.getEsimEnabled()
        switchBar.isEnabled = true
        footerPref.title = getString(R.string.esim_footer_note)
    }

    override fun onResume() {
        super.onResume()
        switchBar.isChecked = esimController.getEsimEnabled()
    }

    override fun onDestroy() {
        executor.shutdownNow()
        super.onDestroy()
    }

    override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {
        if (preference == switchBar) {
            val isChecked = newValue as Boolean
            if (DEBUG) Log.d(TAG, "onPreferenceChange: $isChecked")
            if (esimController.getEsimActive()) {
                if (isChecked) return false
                AlertDialog.Builder(requireContext())
                    .setTitle(R.string.esim_warning_title)
                    .setMessage(R.string.esim_warning_message)
                    .setPositiveButton(android.R.string.ok, null)
                    .setCancelable(false)
                    .show()
                return false
            }

            val gen = requestGen.incrementAndGet()
            switchBar.isEnabled = false
            footerPref.title = getString(R.string.esim_switching)
            executor.execute {
                var ok = false
                try {
                    ok = esimController.setEsimEnabled(isChecked)
                } catch (t: Throwable) {
                    Log.e(TAG, "setEsimEnabled failed", t)
                }
                mainHandler.post {
                    if (!isAdded || gen != requestGen.get()) return@post
                    switchBar.isEnabled = true
                    switchBar.isChecked = esimController.getEsimEnabled()
                    footerPref.title =
                        if (ok) {
                            getString(R.string.esim_footer_note)
                        } else {
                            getString(R.string.esim_switch_failed)
                        }
                }
            }
            // Optimistic UI; corrected when background work finishes.
            return true
        }
        return true
    }
}
