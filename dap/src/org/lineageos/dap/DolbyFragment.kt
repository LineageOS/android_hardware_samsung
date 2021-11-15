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

import androidx.preference.PreferenceFragment

import com.android.settingslib.widget.MainSwitchPreference
import com.android.settingslib.widget.OnMainSwitchChangeListener
import com.android.settingslib.widget.RadioButtonPreference

import org.lineageos.dap.R

class DolbyFragment : PreferenceFragment(), OnMainSwitchChangeListener {

    private lateinit var switchBar: MainSwitchPreference

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        addPreferencesFromResource(R.xml.dolby_settings)

        switchBar = findPreference<MainSwitchPreference>(PREF_DOLBY_ENABLE)!!
        switchBar.addOnSwitchChangeListener(this)
        switchBar.isChecked = DolbyCore.isEnabled()

        for ((key, value) in PREF_DOLBY_MODES) {
            val preference = findPreference<RadioButtonPreference>(key)!!
            preference.setOnPreferenceClickListener {
                setProfile(value)
                true
            }
        }
    }

    override fun onSwitchChanged(switchView: Switch, isChecked: Boolean) {
        DolbyCore.setEnabled(isChecked)
    }

    private fun setProfile(profile: Int) {
        DolbyCore.setProfile(profile)

        for ((key, value) in PREF_DOLBY_MODES) {
            val preference = findPreference<RadioButtonPreference>(key)!!
            preference.isChecked = value == profile
        }
    }

    companion object {
        const val PREF_DOLBY_ENABLE = "dolby_enable"

        val PREF_DOLBY_MODES = mapOf(
                "dolby_profile_auto" to DolbyCore.PROFILE_AUTO,
                "dolby_profile_movie" to DolbyCore.PROFILE_MOVIE,
                "dolby_profile_music" to DolbyCore.PROFILE_MUSIC,
                "dolby_profile_voice" to DolbyCore.PROFILE_VOICE,
                "dolby_profile_game" to DolbyCore.PROFILE_GAME,
                "dolby_profile_off" to DolbyCore.PROFILE_OFF,
                "dolby_profile_game_1" to DolbyCore.PROFILE_GAME_1,
                "dolby_profile_game_2" to DolbyCore.PROFILE_GAME_2,
                "dolby_profile_spacial_audio" to DolbyCore.PROFILE_SPACIAL_AUDIO,
        )
    }
}
