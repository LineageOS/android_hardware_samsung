/*
 * Copyright (C) 2022 The LineageOS Project
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
import android.widget.Switch

import androidx.preference.ListPreference
import androidx.preference.Preference
import androidx.preference.Preference.OnPreferenceChangeListener
import androidx.preference.PreferenceFragment

import com.android.settingslib.widget.MainSwitchPreference
import com.android.settingslib.widget.OnMainSwitchChangeListener

import org.lineageos.dap.R

class DolbyFragment : PreferenceFragment(),
        OnPreferenceChangeListener, OnMainSwitchChangeListener {

    private lateinit var switchBar: MainSwitchPreference

    private var dolbyModesPreference: ListPreference? = null

    private val dolbyCore = DolbyCore()

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        addPreferencesFromResource(R.xml.dolby_settings)

        val dapEnabled = dolbyCore.isEnabled()

        switchBar = findPreference<MainSwitchPreference>(PREF_DOLBY_ENABLE)!!
        switchBar.addOnSwitchChangeListener(this)
        switchBar.isChecked = dapEnabled

        dolbyModesPreference = findPreference<ListPreference>(PREF_DOLBY_MODES)!!
        dolbyModesPreference?.isEnabled = dapEnabled
        dolbyModesPreference?.onPreferenceChangeListener = this
    }

    override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {
        if (preference.key == PREF_DOLBY_MODES) {
            dolbyCore.startDolbyEffect((newValue as String).toInt())
        }
        return true
    }

    override fun onSwitchChanged(switchView: Switch, isChecked: Boolean) {
        dolbyCore.setEnabled(isChecked)

        switchBar.isChecked = isChecked
        dolbyModesPreference?.isEnabled = isChecked
    }

    companion object {
        public const val PREF_DOLBY_ENABLE = "dolby_enable"
        public const val PREF_DOLBY_MODES = "dolby_modes"
    }
}
