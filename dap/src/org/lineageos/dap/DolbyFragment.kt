/*
 * Copyright (c) 2022 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package org.lineageos.dap

import android.os.Bundle
import androidx.preference.ListPreference
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreference
import org.lineageos.dap.R

class DolbyFragment : PreferenceFragmentCompat(), Preference.OnPreferenceChangeListener {
    private var DolbyModesPreference: ListPreference? = null
    private var DolbyEnablePreference: SwitchPreference? = null
    private var dolbyCore = DolbyCore()
    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        addPreferencesFromResource(R.xml.dolby_settings)
        DolbyModesPreference = findPreference(DOLBY_MODES)
        DolbyEnablePreference = findPreference(PREF_DOLBY)
        DolbyEnablePreference!!.isChecked = dolbyCore.isRunning()
        DolbyEnablePreference!!.onPreferenceChangeListener = this
        val items = arrayOf<CharSequence>(
            "1", "2", "3", "4", "5", "6", "7", "8", "9"
        )
        DolbyModesPreference!!.entryValues = items
        DolbyModesPreference!!.onPreferenceChangeListener =
            Preference.OnPreferenceChangeListener { _: Preference?, newValue: Any ->
                when ((newValue as String).toInt()) {
                    1 -> dolbyCore.startDolbyEffect(DolbyCore.PROFILE_AUTO)
                    2 -> dolbyCore.startDolbyEffect(DolbyCore.PROFILE_GAME)
                    3 -> dolbyCore.startDolbyEffect(DolbyCore.PROFILE_GAME_1)
                    4 -> dolbyCore.startDolbyEffect(DolbyCore.PROFILE_GAME_2)
                    5 -> dolbyCore.startDolbyEffect(DolbyCore.PROFILE_MOVIE)
                    6 -> dolbyCore.startDolbyEffect(DolbyCore.PROFILE_MUSIC)
                    7 -> dolbyCore.startDolbyEffect(DolbyCore.PROFILE_OFF)
                    8 -> dolbyCore.startDolbyEffect(DolbyCore.PROFILE_VOICE)
                    9 -> dolbyCore.startDolbyEffect(DolbyCore.PROFILE_SPACIAL_AUDIO)
                    else -> {
                    }
                }
                true
            }
    }

    override fun onPreferenceChange(preference: Preference, newValue: Any): Boolean {
        if (preference === DolbyEnablePreference) {
            val value = newValue as Boolean
            if (value) {
                dolbyCore.justStartOnly()
            } else {
                dolbyCore.stopDolbyEffect()
            }
            return true
        }
        return false
    }

    companion object {
        private const val PREF_DOLBY = "dolby_pref"
        private const val DOLBY_MODES = "dolby_modes"
    }
}