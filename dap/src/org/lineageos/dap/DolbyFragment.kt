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
import androidx.preference.Preference.OnPreferenceChangeListener
import androidx.preference.PreferenceFragment
import androidx.preference.SwitchPreference
import org.lineageos.dap.R

class DolbyFragment : PreferenceFragment() {
    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        addPreferencesFromResource(R.xml.dolby_settings)

        val dolbyCore = DolbyCore()

        val dolbyModesPreference = findPreference<ListPreference>(PREF_DOLBY_MODES)!!
        dolbyModesPreference.onPreferenceChangeListener =
                OnPreferenceChangeListener { _, newValue: Any ->
                    dolbyCore.startDolbyEffect((newValue as String).toInt())
                    true
                }

        val dolbyEnablePreference = findPreference<SwitchPreference>(PREF_DOLBY_ENABLE)!!
        dolbyEnablePreference.isChecked = dolbyCore.isEnabled()
        dolbyEnablePreference.onPreferenceChangeListener =
                OnPreferenceChangeListener { _, newValue: Any ->
                    dolbyCore.setEnabled(newValue as Boolean)
                    true
                }
    }

    companion object {
        private const val PREF_DOLBY_ENABLE = "dolby_enable"
        private const val PREF_DOLBY_MODES = "dolby_modes"
    }
}
